// One short-lived screening room. Only poses, reservations and playback state
// cross this connection; every viewer receives video/audio directly from YouTube.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {WebSocketServer,WebSocket} from 'ws';

const args=Object.fromEntries(process.argv.slice(2).map(v=>{const i=v.indexOf('=');return i<0?[v.replace(/^--/,''),true]:[v.slice(2,i),v.slice(i+1)];}));
if(!args.info || !args.parent)throw new Error('Local info path and owner process are required');
const ownerPid=Number(args.parent),started=Date.now(),startHour=Number.isFinite(Number(args.hour))?Number(args.hour):10;
// Supplied by the game; the 1-hour fallback preserves older host executables.
const daySeconds=Number(args['day-seconds'] ?? 3600);
if(!Number.isFinite(daySeconds) || daySeconds<60 || daySeconds>86400)throw new Error('Invalid city day length');
const hostKey=crypto.randomBytes(24).toString('hex'),joinKey=crypto.randomBytes(24).toString('hex');
const roomId=crypto.randomBytes(8).toString('hex');let hostId=null,hostLeftAt=null,closing=false,tunnel=null;
const peers=new Map(),claims=new Map();let media={id:'',time:0,paused:true,updated:started,revision:0};
const info={version:1,ready:false,roomId,endpoint:'',hostKey,joinKey,invite:'',publicOrigin:'',error:''};
fs.mkdirSync(path.dirname(args.info),{recursive:true});
let infoWrite=0;
function saveInfo(){
    const revision=++infoWrite;
    function attempt(number){
        if(revision!==infoWrite)return;
        try{const temp=args.info+'.tmp';fs.writeFileSync(temp,JSON.stringify(info));fs.renameSync(temp,args.info);}
        catch(error){if(number<50 && ['EPERM','EACCES','EBUSY'].includes(error.code))setTimeout(()=>attempt(number+1),20);else console.error('Unable to write local session status:',error.code);}
    }
    attempt(0);
}
function keyMatches(a,b){return typeof a==='string' && /^[a-f0-9]{48}$/.test(a) && crypto.timingSafeEqual(Buffer.from(a),Buffer.from(b));}
function send(ws,value){if(ws.readyState===WebSocket.OPEN && ws.bufferedAmount<65536)ws.send(JSON.stringify(value));}
function cleanName(value){return String(value||'旅人').replace(/[\x00-\x1f\x7f]/g,'').trim().slice(0,20)||'旅人';}
function finite(value,min,max){return typeof value==='number' && Number.isFinite(value) && value>=min && value<=max;}
function snapshot(){const now=Date.now();return {type:'state',roomId,serverTime:now,daySeconds,dayHour:((startHour+(now-started)*24/(daySeconds*1000))%24+24)%24,media,
    peers:[...peers.values()].map(p=>({id:p.id,name:p.name,host:p.id===hostId,position:p.position,yaw:p.yaw,seat:p.seat,visible:p.visible}))};}
function broadcast(){const state=snapshot();for(const p of peers.values())send(p.ws,state);}
function disconnect(p){
    if(!peers.delete(p.id))return;
    if(p.seat>=0)claims.delete(p.seat);
    if(p.id===hostId){hostId=null;hostLeftAt=Date.now();}
    broadcast();
}
function finish(){
    if(closing)return;closing=true;clearInterval(ticker);clearInterval(lease);
    for(const p of peers.values()){send(p.ws,{type:'ended',message:'ホストが部屋を閉じました。'});p.ws.close(1001,'Room ended');}
    if(tunnel)tunnel.kill();wss.close();server.close();
    info.ready=false;info.invite='';info.hostKey='';info.joinKey='';saveInfo();setTimeout(()=>process.exit(0),400).unref();
}
const server=http.createServer((req,res)=>{res.writeHead(req.url==='/health'?200:404,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(req.url==='/health'?{service:'EndlessWorld screening room',version:1}:{error:'not found'}));});
const wss=new WebSocketServer({noServer:true,maxPayload:8192,perMessageDeflate:false});
server.on('upgrade',(req,socket,head)=>{
    let url;try{url=new URL(req.url,'http://localhost');}catch{socket.destroy();return;}
    const key=url.searchParams.get('key');const role=url.pathname==='/host'?'host':url.pathname==='/join'?'guest':'';
    if(!role || !keyMatches(key,role==='host'?hostKey:joinKey) || peers.size>=32 || role==='host' && hostId || role==='guest' && !hostId)
    {socket.write('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');socket.destroy();return;}
    wss.handleUpgrade(req,socket,head,ws=>{
        const p={ws,id:crypto.randomBytes(8).toString('hex'),name:cleanName(url.searchParams.get('name')),position:[1150,-2100,89],yaw:90,seat:-1,visible:false,
                 windowAt:Date.now(),messages:0,lastSeen:Date.now()};
        peers.set(p.id,p);if(role==='host'){hostId=p.id;hostLeftAt=null;}
        send(ws,{type:'welcome',id:p.id,host:role==='host',roomId,serverTime:Date.now()});broadcast();
        ws.on('pong',()=>{p.lastSeen=Date.now();});
        ws.on('error',()=>disconnect(p));ws.on('close',()=>disconnect(p));
        ws.on('message',(data,isBinary)=>{
            const now=Date.now();if(now-p.windowAt>=1000){p.windowAt=now;p.messages=0;}
            if(isBinary || ++p.messages>45){ws.close(1008,'Message limit');return;}
            let m;try{m=JSON.parse(data.toString());}catch{ws.close(1008,'Invalid message');return;}
            if(!m || typeof m!=='object')return;p.lastSeen=now;
            if(m.type==='ping'){send(ws,{type:'pong',echo:m.echo,serverTime:now});return;}
            if(m.type==='pose'){
                if(!Array.isArray(m.position)||m.position.length!==3 || !finite(m.position[0],-100000,100000) || !finite(m.position[1],-100000,100000) || !finite(m.position[2],-100000,100000)||!finite(m.yaw,-36000,36000))return;
                p.position=m.position;p.yaw=m.yaw;p.visible=!!m.visible;
            }else if(m.type==='release'){
                if(p.seat>=0 && m.index===p.seat){claims.delete(p.seat);p.seat=-1;broadcast();}
            }else if(m.type==='seat'){
                const seat=m.index;if(!Number.isInteger(seat)||seat<0||seat>=32)return;
                const ok=!claims.has(seat)||claims.get(seat)===p.id;
                if(ok){if(p.seat>=0)claims.delete(p.seat);claims.set(seat,p.id);p.seat=seat;}
                send(ws,{type:'seat',index:seat,ok});broadcast();
            }else if(m.type==='media' && p.id===hostId){
                if(typeof m.id!=='string' || m.id!=='' && !/^[A-Za-z0-9_-]{11}$/.test(m.id) || !finite(m.time,0,172800)||typeof m.paused!=='boolean')return;
                media={id:m.id,time:m.time,paused:m.paused,updated:now,revision:media.revision+1};broadcast();
            }else if(m.type==='end' && p.id===hostId){finish();}
        });
    });
});
server.headersTimeout=5000;server.requestTimeout=5000;
server.listen(0,'127.0.0.1',()=>{
    const port=server.address().port;info.endpoint=`ws://127.0.0.1:${port}`;info.ready=true;
    if(args['local-only']){info.invite=`ewcinema1|http://127.0.0.1:${port}|${joinKey}`;saveInfo();return;}
    const bin=path.join(path.dirname(fileURLToPath(import.meta.url)),'cloudflared.exe');
    tunnel=spawn(bin,['tunnel','--no-autoupdate','--url',`http://127.0.0.1:${port}`,'--protocol','http2','--metrics','127.0.0.1:0'],{windowsHide:true,stdio:['ignore','pipe','pipe']});
    let checkingOrigin=false;
    async function publishWhenReady(origin){
        for(let attempt=0;attempt<40 && !closing;attempt++){
            try{const r=await fetch(origin+'/health',{signal:AbortSignal.timeout(3000),redirect:'error'});
                const v=await r.json();if(r.ok && v.service==='EndlessWorld screening room'){
                    if(closing)return;info.publicOrigin=origin;info.invite=`ewcinema1|${origin}|${joinKey}`;saveInfo();return;
                }
            }catch{}
            await new Promise(resolve=>setTimeout(resolve,1000));
        }
        if(!closing){info.error='招待用の接続先を準備できませんでした。部屋を作り直してください。';saveInfo();}
    }
    const observe=data=>{
        const text=data.toString();const match=text.match(/https:\/\/[a-z0-9-]+\.trycloudflare\.com/);
        if(match && !checkingOrigin){checkingOrigin=true;publishWhenReady(match[0]);}
    };
    tunnel.stdout.on('data',observe);tunnel.stderr.on('data',observe);
    tunnel.on('error',()=>{info.error='接続中継を起動できませんでした。';saveInfo();});
    tunnel.on('exit',()=>{if(!closing){info.error='接続中継が終了しました。部屋を作り直してください。';saveInfo();finish();}});
    saveInfo();
});
const ticker=setInterval(()=>broadcast(),100);
const lease=setInterval(()=>{
    try{process.kill(ownerPid,0);}catch{finish();return;}
    if(!hostId && Date.now()-(hostLeftAt||started)>45000){finish();return;}
    for(const p of peers.values()){if(Date.now()-p.lastSeen>15000){p.ws.terminate();disconnect(p);}else p.ws.ping();}
},3000);
process.on('SIGTERM',finish);process.on('SIGINT',finish);
