import http from 'node:http';
import {mkdir,chmod,unlink} from 'node:fs/promises';
import {resolve,dirname} from 'node:path';

export const socketPath=process.env.CML_API_SOCKET||resolve('work/cml-api.sock');
const origin='https://cml.kappino.me';
const isActive=n=>!['DEFINED_ON_CORE','STOPPED'].includes(n.state);
export function requestedNodeCount(nodes,requested=[]){return new Set([...nodes.filter(isActive).map(n=>n.id),...requested]).size;}
const requireId=id=>{if(!/^[a-f\d-]{36}$/i.test(id||''))throw new Error('LAB_ID non valido.');return id;};
export async function startBridge(page){
 if(new URL(page.url()).origin!==origin)throw new Error('La scheda selezionata non è il CML autorizzato.');
 await mkdir(dirname(socketPath),{recursive:true});
 // Do not replace a socket belonging to another live bridge.
 let busy=false;
 const server=http.createServer(async(req,res)=>{
  let ownsLock=false;
  try{
   if(req.method!=='POST'||req.url!=='/api')throw new Error('Usare POST /api.');
   let input='';for await(const chunk of req){input+=chunk;if(input.length>2_000_000)throw new Error('Richiesta troppo grande.');}
   const {operation,lab_id,node,command,topology}=JSON.parse(input);
   if(['import','start','stop','console'].includes(operation)){if(busy)throw new Error('Un’altra operazione CML è in corso.');busy=true;ownsLock=true;}
   const api=async(path,method='GET',body)=>{
    let result;
    for(let attempt=0;attempt<3;attempt++){
    result=await page.evaluate(async({path,method,body})=>{
     if(location.origin!=='https://cml.kappino.me')return {status:409,data:'La scheda Chrome ha cambiato sito.'};
     const auth=JSON.parse(localStorage.getItem('auth')||'{}');if(!auth.authtoken)return {status:401,data:'Login CML necessario.'};
     const r=await fetch('/api/v0'+path,{method,headers:{Authorization:'Bearer '+auth.authtoken,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(20000)});
     const text=await r.text();let data;try{data=JSON.parse(text)}catch{data=text}return {status:r.status,data};
    },{path,method,body});
    if(method!=='GET'||![502,503,504].includes(result.status)||attempt===2)break;
    await new Promise(resolve=>setTimeout(resolve,500*(attempt+1)));
    }
    if(result.status>=400)throw new Error(`CML HTTP ${result.status}: ${typeof result.data==='object'?JSON.stringify(result.data).slice(0,180):'servizio remoto non disponibile'}`);
    return result.data;
   };
   let result;
   if(operation==='health'||operation==='labs'){
    const labs=await api('/labs?show_all=true&with_data=true');
    result={connected:true,origin,labs:labs.map(l=>({id:l.id,title:l.lab_title??l.title,state:l.state})),limit:5};
    if(operation==='health')result.active_nodes=(await api('/nodes')).filter(isActive).map(n=>({id:n.id,label:n.label,lab_id:n.lab_id,state:n.state}));
   }else if(operation==='import'){
    validateTopology(topology);
    const labs=await api('/labs?show_all=true&with_data=true');
    if(labs.some(l=>(l.lab_title??l.title)===topology.lab.title))throw new Error('Titolo già presente: nessuna sovrascrittura.');
    result=await api('/import','POST',topology);
   }else if(operation==='start'){
    requireId(lab_id);const active=await api('/nodes');const requested=await api(`/labs/${lab_id}/nodes`);
    const count=requestedNodeCount(active,requested);
    if(count>5)throw new Error(`Avvio rifiutato: ${count} nodi complessivi, limite 5. Fermare esplicitamente un altro lab.`);
    await api(`/labs/${lab_id}/start`,'PUT');result={started:lab_id,maximum_nodes:count};
   }else if(operation==='stop'){
    requireId(lab_id);await api(`/labs/${lab_id}/stop`,'PUT');result={stopped:lab_id};
   }else if(operation==='console'){
    requireId(lab_id);if(typeof command!=='string'||command.length>1000||/[\r\n]/.test(command))throw new Error('Inviare una sola riga di comando.');
    const ids=await api(`/labs/${lab_id}/nodes`);let selected;
    for(const id of ids){const n=await api(`/labs/${lab_id}/nodes/${id}`);if(n.label===node||n.id===node){selected=n;break;}}
    if(!selected)throw new Error('Nodo non trovato nel laboratorio.');
    if(!['STARTED','BOOTED'].includes(selected.state))throw new Error('Nodo non acceso.');
    const key=await api(`/labs/${lab_id}/nodes/${selected.id}/keys/console`);
    result=await page.evaluate(async({key,command})=>await new Promise(resolve=>{
     const ws=new WebSocket('wss://'+location.host+'/ws/dispatch/frontend/console?cml_client=WebUI&action=lab_exec&uuid='+encodeURIComponent(key));
     let output='',finished=false;const timers=[];
     const finish=error=>{if(finished)return;finished=true;timers.forEach(clearTimeout);ws.close();resolve({ok:!error,source:'cml_console',output,error:error||null});};
     timers.push(setTimeout(()=>finish('Timeout connessione console.'),15000));
     ws.onopen=()=>{ws.send(JSON.stringify({token:JSON.parse(localStorage.auth).authtoken}));timers.push(setTimeout(()=>{if(ws.readyState===WebSocket.OPEN)ws.send('\r');},500));timers.push(setTimeout(()=>{if(ws.readyState===WebSocket.OPEN)ws.send(command+'\r');},1500));timers.push(setTimeout(()=>finish(output.trim()?null:'Console connessa ma nessun output ricevuto; esecuzione non verificata.'),8000));};
     ws.onmessage=async e=>{output+=typeof e.data==='string'?e.data:await e.data.text();};
     ws.onerror=()=>finish('Trasporto console non disponibile.');
     ws.onclose=()=>finish(output.trim()?null:'Console chiusa senza output.');
    }),{key,command});
   }else throw new Error('Operazione sconosciuta.');
   res.writeHead(200,{'Content-Type':'application/json'});res.end(JSON.stringify(result));
  }catch(e){res.writeHead(400,{'Content-Type':'application/json'});res.end(JSON.stringify({ok:false,error:e.message}));}finally{if(ownsLock)busy=false;}
 });
 await new Promise((ok,fail)=>{server.once('error',fail);server.listen(socketPath,ok);});await chmod(socketPath,0o600);
 server.on('close',()=>unlink(socketPath).catch(()=>{}));return server;
}
export function validateTopology(t){
 if(!t?.lab?.title||!Array.isArray(t.nodes)||!Array.isArray(t.links)||t.nodes.length<1||t.nodes.length>5)throw new Error('Topologia CML non valida o oltre 5 nodi.');
 const nodes=new Map(t.nodes.map(n=>[n.id,n]));if(nodes.size!==t.nodes.length)throw new Error('ID nodo duplicato.');
 const used=new Set();
 for(const l of t.links)for(const [n,i]of [['n1','i1'],['n2','i2']]){
  if(!nodes.get(l[n])?.interfaces?.some(p=>p.id===l[i]))throw new Error('Link verso una porta inesistente.');
  const key=l[n]+':'+l[i];if(used.has(key))throw new Error('Porta occupata da due link.');used.add(key);
 }
}
