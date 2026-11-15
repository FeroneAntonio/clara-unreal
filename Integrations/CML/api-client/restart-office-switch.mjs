import {readFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {join} from 'node:path';
import puppeteer from 'puppeteer-core';
const [port,path]=(await readFile(join(homedir(),'Library/Application Support/Google/Chrome/DevToolsActivePort'),'utf8')).trim().split('\n');
const browser=await puppeteer.connect({browserWSEndpoint:`ws://127.0.0.1:${port}${path}`,defaultViewport:null});
try{
 const page=(await browser.pages()).find(p=>p.url().startsWith('https://cml.kappino.me/'));
 console.log(await page.evaluate(async()=>{
  if(location.origin!=='https://cml.kappino.me')throw Error('Origine errata');
  const token=JSON.parse(localStorage.auth).authtoken;
  const base='/labs/1fc0e59b-0dfe-4908-8cae-c7bc470a0471/nodes/4dd95325-a4f7-4b59-bbbb-896aad60051b';
  const api=async(path,method='GET')=>{const r=await fetch('/api/v0'+path,{method,headers:{Authorization:'Bearer '+token},signal:AbortSignal.timeout(20000)});const t=await r.text();if(!r.ok)throw Error(`CML ${r.status} ${t}`);return t?JSON.parse(t):null;};
  const n=await api(base);if(n.label!=='SW-PIANO1')throw Error('Nodo diverso');
  await api(base+'/state/stop','PUT');
  for(let i=0;i<30;i++){if((await api(base)).state==='STOPPED')break;await new Promise(r=>setTimeout(r,1000));if(i===29)throw Error('Timeout stop');}
  const active=(await api('/nodes')).filter(n=>!['STOPPED','DEFINED_ON_CORE'].includes(n.state)).length;
  if(active>=5)throw Error('Limite nodi');
  await api(base+'/state/start','PUT');return {restarted:'SW-PIANO1',maximum_nodes:active+1};
 }));
}finally{await browser.disconnect();}
