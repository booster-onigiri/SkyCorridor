"""Run silent packaged-game regression cases with isolated saves and reports."""
from pathlib import Path
import argparse, hashlib, json, os, sqlite3, subprocess, time

CASES={
 'explore':(['-EWExplore85Audit'],1000,'explore'),
 'explore-resume':(['-EWExplore85Audit','-EWExplore85Resume'],240,'explore'),
 'memories':(['-EWMemory89Audit'],720,'memories'),
 'memories-resume':(['-EWMemory89Audit','-EWMemoryResume'],240,'memories'),
 'upper-rail':(['-EWSkyrailAudit','-EWUpper88'],1440,'upper-rail'),
 'airship':(['-EWAero87Audit','-EWRebaseEachChunk'],1080,'airship'),
 'airship-resume':(['-EWAero87Audit','-EWAero87Resume'],240,'airship'),
 'media':(['-EWMediaAudit'],360,'media'),
}

POSITION_RESUME_CASES={'explore-resume','airship-resume'}


def read_audit_report(path):
 data=path.read_bytes()
 # Unreal's SaveStringToFile(AutoDetect) selects UTF-16 for non-ASCII text.
 encoding='utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'
 return json.loads(data.decode(encoding))

def launch_mode(name):
 if name in POSITION_RESUME_CASES:return 'title_then_continue'
 if name=='memories-resume':return 'auto_start_journal_only'
 return 'auto_start'

def case_scope(name):
 if name in POSITION_RESUME_CASES:
  return 'Start at the normal title preview, then exercise ContinueWorld and saved position restoration.'
 if name=='memories-resume':
  return 'Restore the separate remembrance journal only; does not test avatar position restoration.'
 return 'Isolated scripted runtime case; physical inputs and listening are not tested.'

def build_launch_args(exe,out,name):
 flags,_,save=CASES[name];case=out/name;data=out/'Saves'/save
 # EWPlay starts a fresh plaza session and saves it when the preview finishes.
 # Position-resume audits must use the same title/Continue path as PLAY.cmd.
 args=[str(exe)]+([] if name in POSITION_RESUME_CASES else ['-EWPlay'])
 return args+['-EWSilentAudit','-unattended','-nosplash','-RenderOffscreen',
        '-windowed','-ResX=1920','-ResY=1080','-ForceRes',
        '-EWDataDir='+str(data),'-UserDir='+str(out/'User'/save),
        '-EWReport='+str(case/'audit.json'),'-abslog='+str(case/'game.log')]+flags

def snapshot_resume_save(out,data,name):
 # Only inspect this runner's isolated fixture. SQLite is opened read-only.
 out=out.resolve();data=data.resolve();save_root=out/'Saves'
 if not data.is_relative_to(save_root):raise ValueError('save snapshot escapes isolated audit directory')
 db=(data/'Remembrance'/'exploration.sqlite3') if name=='memories-resume' else data/'exploration.sqlite3'
 db=db.resolve()
 if not db.is_relative_to(save_root):raise ValueError('save database escapes isolated audit directory')
 result={'file':str(db.relative_to(out)),'exists':db.is_file(),'read_only':True,
         'scope':'journal_only' if name=='memories-resume' else 'saved_position_and_discoveries'}
 if not result['exists']:return result
 before=hashlib.sha256(db.read_bytes()).hexdigest()
 result.update(sha256=before,bytes=db.stat().st_size)
 connection=sqlite3.connect(db.as_uri()+'?mode=ro',uri=True)
 try:
  connection.execute('PRAGMA query_only=ON')
  result['positions']=[dict(zip(('world','id','cx','cy','x','y','z','yaw'),row)) for row in
     connection.execute('SELECT world,id,cx,cy,x,y,z,yaw FROM positions ORDER BY world')]
  result['discovery_count']=connection.execute('SELECT COUNT(*) FROM discoveries').fetchone()[0]
  result['last_world']=connection.execute("SELECT value FROM metadata WHERE name='last_world'").fetchall()
 finally:connection.close()
 result['sha256_after_read']=hashlib.sha256(db.read_bytes()).hexdigest()
 if result['sha256_after_read']!=before:raise RuntimeError('save database changed during read-only snapshot')
 return result

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--exe',type=Path,required=True)
 p.add_argument('--output',type=Path,required=True)
 p.add_argument('--cases',nargs='+',choices=CASES,default=['memories','memories-resume','explore','explore-resume','upper-rail','airship','airship-resume','media'])
 a=p.parse_args();exe=a.exe.resolve();out=a.output.resolve()
 if not exe.is_file():raise FileNotFoundError(exe)
 out.mkdir(parents=True,exist_ok=False)
 results=[]
 def receipt(status='running'):
  (out/'verification.json').write_text(json.dumps({'success':status=='complete' and len(results)==len(a.cases) and all(r['success'] for r in results),
    'status':status,'expected_cases':a.cases,'cases':results,
    'scope':'Isolated scripted CharacterMovement and offscreen GPU checks. Physical inputs, listening, HDR monitor and online services are separate.'},indent=2),encoding='utf-8')
 receipt()
 for name in a.cases:
  flags,timeout,save=CASES[name]
  case=out/name;case.mkdir();report=case/'audit.json';data=out/'Saves'/save
  args=build_launch_args(exe,out,name)
  entry={'case':name,'report':name+'/audit.json','launch_mode':launch_mode(name),
         'launch_args':args,'case_scope':case_scope(name)}
  if name.endswith('-resume'):
   try:entry['pre_resume_save']=snapshot_resume_save(out,data,name)
   except (OSError,sqlite3.Error,ValueError,RuntimeError) as error:
    entry.update(success=False,failure='pre-resume save snapshot: '+str(error),exit_code=None)
    results.append(entry);receipt('failed');raise SystemExit(1)
  (case/'launch.json').write_text(json.dumps(entry,indent=2),encoding='utf-8')
  start=time.monotonic()
  startup=None
  if os.name=='nt':
   startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
  with (case/'console.log').open('wb') as log:
   proc=subprocess.Popen(args,cwd=exe.parent,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
   try:code=proc.wait(timeout=timeout)
   except subprocess.TimeoutExpired:
    proc.terminate();code=proc.wait(timeout=30)
  entry.update(exit_code=code,seconds=time.monotonic()-start)
  if report.is_file():
   try:
    result=read_audit_report(report)
    entry.update(success=code==0 and result.get('success',False),failure=result.get('failure',result.get('error','')),
                 checks=len(result.get('checks',[])),report_sha256=hashlib.sha256(report.read_bytes()).hexdigest())
   except (OSError,UnicodeError,json.JSONDecodeError) as error:
    entry.update(success=False,failure='invalid audit report: '+str(error))
  else:entry.update(success=False,failure='no complete audit report')
  results.append(entry)
  receipt('failed' if not entry['success'] else 'running')
  print(json.dumps(entry),flush=True)
  if not entry['success']:raise SystemExit(1)
 receipt('complete')

if __name__=='__main__':main()
