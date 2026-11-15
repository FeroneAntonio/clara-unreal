import {readFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {join} from 'node:path';
import puppeteer from 'puppeteer-core';
import {startBridge,socketPath} from './bridge.mjs';
const [port,path]=(await readFile(process.env.CML_CHROME_PORT_FILE||join(homedir(),'Library/Application Support/Google/Chrome/DevToolsActivePort'),'utf8')).trim().split('\n');
const browser=await puppeteer.connect({browserWSEndpoint:`ws://127.0.0.1:${port}${path}`,defaultViewport:null});
try{
 const page=(await browser.pages()).find(p=>p.url().startsWith('https://cml.kappino.me/'));
 if(!page)throw new Error('CML non aperto nel Chrome abituale.');
 const server=await startBridge(page);console.log('API locale CML pronta:',socketPath);
 for(const sig of ['SIGINT','SIGTERM'])process.once(sig,()=>server.close(async()=>{await browser.disconnect();process.exit(0);}));
}catch(e){await browser.disconnect();throw e;}
