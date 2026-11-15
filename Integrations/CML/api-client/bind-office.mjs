import {readFile,writeFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {join} from 'node:path';
import puppeteer from 'puppeteer-core';
const lab='1fc0e59b-0dfe-4908-8cae-c7bc470a0471';
const node='4dd95325-a4f7-4b59-bbbb-896aad60051b';
const [port,path]=(await readFile(join(homedir(),'Library/Application Support/Google/Chrome/DevToolsActivePort'),'utf8')).trim().split('\n');
const browser=await puppeteer.connect({browserWSEndpoint:`ws://127.0.0.1:${port}${path}`,defaultViewport:null});
try {
 const page=(await browser.pages()).find(p=>p.url().startsWith('https://cml.kappino.me/'));
 if(!page)throw Error('Scheda CML mancante');
 const result=await page.evaluate(async({lab,node})=>{
  if(location.origin!=='https://cml.kappino.me')throw Error('Origine non autorizzata');
  const token=JSON.parse(localStorage.getItem('auth')||'{}').authtoken;
  if(!token)throw Error('Autenticazione necessaria');
  const api=async(path,method='GET',body)=>{
   const r=await fetch('/api/v0'+path,{method,headers:{Authorization:'Bearer '+token,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(20000)});
   const data=await r.json();if(!r.ok)throw Error(`CML ${r.status}: ${JSON.stringify(data)}`);return data;
  };
  const labs=await api('/labs?show_all=true&with_data=true');
  if(labs.some(l=>l.id!==lab&&(l.lab_title??l.title)==='FuoriRete_Room_Office'))throw Error('Laboratorio Office già presente: nessuna modifica');
  const beforeLab=await api(`/labs/${lab}`);
  const beforeNode=await api(`/labs/${lab}/nodes/${node}`);
  const oldTitle=beforeLab.lab_title??beforeLab.title;
  if(!['TNES N1 - soluzione','FuoriRete_Room_Office'].includes(oldTitle)||!['SW1','SW-PIANO1'].includes(beforeNode.label))throw Error('Identità laboratorio o nodo diversa dal previsto');
  await api(`/labs/${lab}`,'PATCH',{title:'FuoriRete_Room_Office'});
  try {await api(`/labs/${lab}/nodes/${node}`,'PATCH',{label:'SW-PIANO1'});}
  catch(e){await api(`/labs/${lab}`,'PATCH',{title:oldTitle});throw e;}
  return {lab_id:lab,node_id:node,before:{title:oldTitle,label:beforeNode.label},after:{title:'FuoriRete_Room_Office',label:'SW-PIANO1'},active_nodes:(await api('/nodes')).filter(n=>!['STOPPED','DEFINED_ON_CORE'].includes(n.state)).length};
 },{lab,node});
 await writeFile(new URL('../gateway-office-binding.json',import.meta.url),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}finally{await browser.disconnect();}
