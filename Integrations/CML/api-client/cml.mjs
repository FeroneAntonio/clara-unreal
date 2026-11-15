import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {parse} from 'yaml';
import {socketPath,validateTopology} from './bridge.mjs';
const [operation,...args]=process.argv.slice(2);
try{
 if(!operation||operation==='help'){
  console.log('node cml.mjs health | labs | import FILE.yaml | start LAB_ID | stop LAB_ID | console LAB_ID NODO "COMANDO"');
 }else{
  const body={operation};
  if(['start','stop','console'].includes(operation)){if(!args[0])throw new Error('LAB_ID mancante.');body.lab_id=args[0];}
  if(operation==='console'){if(!args[1]||!args[2])throw new Error('Nodo o comando mancanti.');Object.assign(body,{node:args[1],command:args[2]});}
  if(operation==='import'){if(!args[0])throw new Error('File YAML mancante.');body.topology=parse(await readFile(args[0],'utf8'));validateTopology(body.topology);}
  const result=await new Promise((resolve,reject)=>{
   const req=http.request({socketPath,path:'/api',method:'POST',headers:{'Content-Type':'application/json'}},res=>{let output='';res.on('data',c=>output+=c);res.on('end',()=>{try{const r=JSON.parse(output);res.statusCode>=400||r.ok===false?reject(new Error(r.error||'Operazione fallita.')):resolve(r);}catch(e){reject(e);}});});
   req.setTimeout(30000,()=>req.destroy(new Error('Timeout API CML.')));req.on('error',reject);req.end(JSON.stringify(body));
  });
  console.log(JSON.stringify(result,null,2));
 }
}catch(e){console.error(JSON.stringify({ok:false,error:e.message}));process.exitCode=1;}
