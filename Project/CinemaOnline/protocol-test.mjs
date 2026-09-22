import fs from 'node:fs';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {WebSocket} from 'ws';
const directory=process.argv[2],publicTest=process.argv.includes('--public');fs.mkdirSync(directory,{recursive:true});
const legacyDay=process.argv.includes('--legacy-day'),daySeconds=legacyDay?3600:1800;
const infoPath=path.join(directory,'private-session.json');const checks=[];const sockets=[];
const server=spawn(process.execPath,[new URL('./server.mjs',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1'),'--info='+infoPath,'--parent='+process.pid,'--hour=0',...(legacyDay?[]:['--day-seconds=1800']),...(publicTest?[]:['--local-only'])],{windowsHide:true,stdio:['ignore','pipe','pipe']});
server.stderr.on('data',d=>fs.appendFileSync(path.join(directory,'server-errors.log'),d));
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,timeout=15000){const start=Date.now();while(Date.now()-start<timeout){try{const v=fn();if(v)return v;}catch{}await sleep(50);}throw new Error('Timed out');}
function check(value,name){checks.push({name,pass:!!value});if(!value)throw new Error(name);}
async function connect(url){return new Promise((resolve,reject)=>{const ws=new WebSocket(url);const p={ws,messages:[]};sockets.push(ws);ws.on('message',d=>p.messages.push(JSON.parse(d)));ws.once('open',()=>resolve(p));ws.once('error',reject);});}
function send(p,m){p.ws.send(JSON.stringify(m));}
function latest(p,type){return p.messages.findLast(m=>m.type===type);}
try{
    let info=await until(()=>{const v=JSON.parse(fs.readFileSync(infoPath));return v.ready&&v.endpoint?v:null;});
    const host=await connect(info.endpoint+'/host?key='+info.hostKey+'&name=Host');await until(()=>latest(host,'welcome'));
    if(publicTest)info=await until(()=>{const v=JSON.parse(fs.readFileSync(infoPath));return v.publicOrigin?v:null;},120000);
    const endpoint=publicTest?info.publicOrigin.replace('https:','wss:'):info.endpoint;
    const a=await connect(endpoint+'/join?key='+info.joinKey+'&name=GuestA');await until(()=>latest(a,'welcome'));
    const b=await connect(endpoint+'/join?key='+info.joinKey+'&name=GuestB');await until(()=>latest(b,'state')?.peers.length===3);
    check(latest(b,'state').dayHour<1,'midnight phase preserved and shared');
    const clockBefore=latest(b,'state');
    check(clockBefore.daySeconds===daySeconds,'host day length is sent to every viewer');
    await until(()=>latest(b,'state')?.serverTime>=clockBefore.serverTime+1000);
    const clockAfter=latest(b,'state');
    check(Math.abs((clockAfter.dayHour-clockBefore.dayHour)-(clockAfter.serverTime-clockBefore.serverTime)/(legacyDay?150000:75000))<1e-9,'shared sky advances at the host day rate');
    send(a,{type:'seat',index:7});await until(()=>latest(a,'seat'));check(latest(a,'seat').ok,'guest can reserve a seat');
    send(a,{type:'pose',position:[0,-750,202],yaw:90,visible:true,seat:-1});await sleep(120);
    send(b,{type:'seat',index:7});await until(()=>latest(b,'seat'));check(!latest(b,'seat').ok,'simultaneous seat conflict rejected');
    send(a,{type:'release',index:7});await sleep(120);
    send(b,{type:'seat',index:7});await until(()=>latest(b,'seat')?.ok);check(latest(b,'seat').ok,'standing releases a reserved seat');
    const before=latest(a,'state').media.revision;
    send(a,{type:'media',id:'EWTEST00001',time:5,paused:false});await sleep(250);check(latest(a,'state').media.revision===before,'guest cannot change the host playback');
    send(host,{type:'media',id:'EWTEST00001',time:25,paused:true});await until(()=>latest(b,'state')?.media.time===25);
    check(latest(b,'state').media.paused && latest(a,'state').media.time===25,'host seek and pause broadcast to every viewer');
    send(host,{type:'media',id:'bad<script>',time:27,paused:false});await sleep(250);check(latest(a,'state').media.time===25,'invalid video ID rejected');
    send(a,{type:'pose',position:[2e9,0,0],yaw:0,visible:true,seat:-1});await sleep(250);
    check(latest(b,'state').peers.find(p=>p.name==='GuestA').position[0]===0,'invalid position does not replace last valid pose');
    let rejected=false;try{await connect(endpoint+'/join?key='+'0'.repeat(48));}catch{rejected=true;}check(rejected,'wrong invitation secret rejected');
    rejected=false;try{await connect(endpoint+'/join?key='+encodeURIComponent('あ'.repeat(48)));}catch{rejected=true;}
    check(rejected && server.exitCode===null,'non-ASCII secret rejected without crashing the host');
    b.ws.close();await until(()=>latest(a,'state')?.peers.length===2);check(latest(a,'state').peers.length===2,'disconnect removes guest and reservation');
    const replacements=await connect(endpoint+'/join?key='+info.joinKey+'&name=GuestC');await until(()=>latest(replacements,'welcome'));
    send(replacements,{type:'seat',index:7});await until(()=>latest(replacements,'seat'));check(latest(replacements,'seat').ok,'disconnected seat is reusable');
    send(host,{type:'end'});await until(()=>latest(a,'ended'));check(true,'host closure ends the screening');
    await until(()=>server.exitCode!==null);check(server.exitCode===0,'helper exits cleanly');
    fs.writeFileSync(path.join(directory,'result.json'),JSON.stringify({success:true,public_tls:publicTest,checks},null,2));console.log(JSON.stringify({success:true,public_tls:publicTest,checks:checks.length}));
}catch(error){fs.writeFileSync(path.join(directory,'result.json'),JSON.stringify({success:false,error:String(error),checks},null,2));console.error(error);process.exitCode=1;}
finally{for(const ws of sockets)ws.terminate();if(server.exitCode===null)server.kill();}
