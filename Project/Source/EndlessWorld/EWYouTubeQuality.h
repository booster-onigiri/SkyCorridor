#pragma once
#include "CoreMinimal.h"

namespace EWYouTubeQuality
{
// Use the real watch-page settings menu. The public IFrame quality setters are
// deprecated no-ops. One default per video; trusted user choices take priority.
inline const TCHAR* Script()
{
    return TEXT(R"EWJS(
(()=>{
 const p=document.getElementById('movie_player'),v=p&&p.querySelector('video');
 const prior=window.__EW_QUALITY83;
 const visible=e=>!!e&&e.getBoundingClientRect().width>0&&e.getBoundingClientRect().height>0&&getComputedStyle(e).display!=='none'&&getComputedStyle(e).visibility!=='hidden';
 if(!p||!v)return;
 let id='';try{id=(p.getVideoData?p.getVideoData().video_id:'')||new URL(location.href).searchParams.get('v')||'';}catch(_){}
 if(!id)return;
 const now=performance.now(),ad=p.classList.contains('ad-showing')||p.classList.contains('ad-interrupting');
 if(ad){if(prior){prior.ad=true;if(prior.owned){const b=p.querySelector('.ytp-settings-button');if(b&&b.getAttribute('aria-expanded')==='true')b.click();prior.owned=false;}}return;}
 let s=prior;
 if(!s||s.id!==id||s.player!==p){s={id,player:p,phase:'waiting',since:now,attempts:0,owned:false,manual:false,ad:false,label:'',height:0,available:[],changes:0};window.__EW_QUALITY83=s;}
 if(!window.__EW_QUALITY83_LISTENERS){
  window.__EW_QUALITY83_LISTENERS=true;
  const manual=e=>{const q=window.__EW_QUALITY83;if(!q||!e.isTrusted||!e.target.closest)return;if(e.target.closest('.ytp-settings-menu,.ytp-settings-button')){q.manual=true;q.owned=false;q.phase='manual';}};
  document.addEventListener('pointerdown',manual,true);document.addEventListener('keydown',manual,true);
 }
 if(s.ad){s.ad=false;if(!s.manual){s.phase='waiting';s.since=now;s.attempts=0;}}
 if(s.manual||s.phase==='selected'||s.phase==='unavailable')return;
 const menu=p.querySelector('.ytp-settings-menu'),gear=p.querySelector('.ytp-settings-button');
 const close=()=>{if(s.owned&&gear&&visible(menu))gear.click();s.owned=false;};
 const fail=()=>{close();s.phase=s.attempts<2?'waiting':'unavailable';s.since=now;};
 if(s.phase==='waiting'){
  if(now-s.since<1200||v.readyState<1||!gear||visible(menu))return;
  s.attempts++;s.owned=true;s.phase='settings';s.since=now;gear.click();return;
 }
 if(now-s.since>6000){fail();return;}
 if(!visible(menu))return;
 const rows=Array.from(menu.querySelectorAll('.ytp-menuitem')).filter(visible);
 if(s.phase==='settings'){
  const row=rows.find(e=>{const l=e.querySelector('.ytp-menuitem-label');return /^(画質|quality|qualité|qualità|qualität|calidad|qualidade|화질|畫質|清晰度|качество)$/i.test((l?l.textContent:e.textContent).trim());});
  if(row){s.phase='qualities';s.since=now;row.click();}return;
 }
 const choices=rows.map(e=>{
  const label=e.textContent.replace(/\s+/g,' ').trim(),m=label.match(/(\d{3,4})p(?:\s*(\d{2,3}))?/i);
  const locked=e.getAttribute('aria-disabled')==='true'||e.hasAttribute('disabled')||/premium|プレミアム|enhanced bitrate/i.test(label)||!!e.querySelector('.ytp-premium-label');
  return m&&!locked?{node:e,label,height:Number(m[1]),fps:Number(m[2]||0)}:null;
 }).filter(Boolean).sort((a,b)=>b.height-a.height||b.fps-a.fps);
 if(!choices.length){
  const advanced=rows.find(e=>/^(詳細設定|advanced)$/i.test(e.textContent.trim()));
  if(advanced&&s.phase!=='advanced'){s.phase='advanced';s.since=now;advanced.click();}return;
 }
 const best=choices[0];s.available=choices.map(x=>x.label);s.label=best.label;s.height=best.height;
 if(best.node.getAttribute('aria-checked')!=='true'){best.node.click();s.changes++;}
 close();s.phase='selected';
})();
)EWJS");
}
}
