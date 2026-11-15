import {readFile,writeFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {join} from 'node:path';
import puppeteer from 'puppeteer-core';
const [port,path]=(await readFile(join(homedir(),'Library/Application Support/Google/Chrome/DevToolsActivePort'),'utf8')).trim().split('\n');
const browser=await puppeteer.connect({browserWSEndpoint:`ws://127.0.0.1:${port}${path}`,defaultViewport:null});
try{
 const page=(await browser.pages()).find(p=>p.url().startsWith('https://cml.kappino.me/'));
 const result=await page.evaluate(async()=>{
  if(location.origin!=='https://cml.kappino.me')throw Error('Origine errata');
  const token=JSON.parse(localStorage.auth).authtoken;
  const lab='1fc0e59b-0dfe-4908-8cae-c7bc470a0471';
  const api=async path=>{const r=await fetch('/api/v0'+path,{headers:{Authorization:'Bearer '+token},signal:AbortSignal.timeout(20000)});const t=await r.text();let data;try{data=JSON.parse(t)}catch{data=t}return {status:r.status,data};};
  const ids=await api(`/labs/${lab}/nodes`);const result=[];
  for(const id of ids.data){const n=await api(`/labs/${lab}/nodes/${id}`);const log=await api(`/labs/${lab}/nodes/${id}/consoles/0/log`);result.push({node:n.data.label,state:n.data.state,boot_progress:n.data.boot_progress,log});}
  return result;
 });
 await writeFile(new URL('../office-console-diagnostic.json',import.meta.url),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}finally{await browser.disconnect();}
