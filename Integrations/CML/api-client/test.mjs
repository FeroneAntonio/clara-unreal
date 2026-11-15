import assert from 'node:assert/strict';
import {requestedNodeCount,validateTopology} from './bridge.mjs';
import {readFile} from 'node:fs/promises';
import {parse} from 'yaml';
// Regression: CML imports report DEFINED_ON_CORE, which must not consume running slots.
const nodes=[...Array.from({length:35},(_,i)=>({id:'off'+i,state:'DEFINED_ON_CORE'})),...Array.from({length:4},(_,i)=>({id:'on'+i,state:'BOOTED'}))];
assert.equal(requestedNodeCount(nodes),4);
assert.equal(requestedNodeCount(nodes,['on0','on1','on2','on3']),4);
assert.equal(requestedNodeCount(nodes,['new1','new2','new3','new4','new5']),9);
assert.equal(requestedNodeCount([{id:'q',state:'QUEUED'}],['new1','new2','new3','new4']),5);
const t=parse(await readFile(new URL('../network-1-soluzione.yaml',import.meta.url),'utf8'));validateTopology(t);
const conflict=structuredClone(t);conflict.links.push({...conflict.links[0],id:'duplicate'});assert.throws(()=>validateTopology(conflict),/Porta occupata/);
const overflow=structuredClone(t);overflow.nodes.push({...t.nodes[0],id:'extra1'},{...t.nodes[0],id:'extra2'});assert.throws(()=>validateTopology(overflow),/5 nodi/);
console.log('PASS: real CML states, idempotent start, running-node limit, occupied-port conflict, YAML node limit.');
