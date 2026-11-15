import sys,json,copy,textwrap
from pathlib import Path
import yaml,jsonschema
BASE=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else BASE/'generated'
OUT.mkdir(parents=True,exist_ok=True)
DEFS={d['id']:d for d in json.load(open(BASE/'reference'/'node-definitions.json'))['data']}
IMAGES={d['node_definition_id']:d['id'] for d in json.load(open(BASE/'reference'/'image-definitions.json'))['data']}
SCHEMA=json.load(open(BASE/'reference'/'cml-openapi.json'));ROOT={'$ref':'#/components/schemas/Topology',**SCHEMA}
MANIFEST=[]
def block(s):return textwrap.dedent(s).strip()+'\n'
def node(label,kind,config,x,y):
 d=DEFS[kind]['device']['interfaces'];ifs=[]
 if d.get('has_loopback_zero'):ifs=[dict(id='i0',label='Loopback0',type='loopback')]
 for i,name in enumerate(d['physical'][:max(d.get('min_count',1),d.get('default_count',1))]):ifs.append(dict(id='i'+str(len(ifs)),label=name,type='physical',slot=i))
 return dict(id=label,label=label,node_definition=kind,image_definition=IMAGES[kind],x=x,y=y,configuration=[dict(name='node.cfg' if kind=='alpine' else 'ios_config.txt',content=block(config))],interfaces=ifs,tags=['tnes'])
def host(label,ip,x,y):return node(label,'alpine',f'''hostname {label}
USERNAME=cisco
PASSWORD=cisco
ip link set eth0 up
ip addr replace {ip}/24 dev eth0
ip route replace default via 192.168.1.1
''',x,y)
def switch(label,ip=None,sol=False):
 s=f'hostname {label}\nno ip domain lookup\nno ip routing\nspanning-tree mode pvst\n'
 for i in range(4):s+=f'interface Ethernet0/{i}\n switchport mode access\n no shutdown\n'
 if ip:s+=f'interface Vlan1\n ip address {ip} 255.255.255.0\n no shutdown\nip default-gateway 192.168.1.1\n'
 if sol:s+='enable secret Privato1\nservice password-encryption\nusername admin secret Personale1\nip domain-name clara.local\nip ssh version 2\nbanner motd #Accesso autorizzato#\nline con 0\n password Console1\n login\n logging synchronous\nline vty 0 15\n login local\n transport input ssh\n'
 else:s+='line con 0\n logging synchronous\n'
 return s+'end\n'
def router(label,interfaces,routes=''):
 s=f'hostname {label}\nno ip domain lookup\nip routing\n'
 for i in range(4):
  ip,mask,up=interfaces.get(i,(None,None,False));s+=f'interface Ethernet0/{i}\n'+(f' ip address {ip} {mask}\n' if ip else ' no ip address\n')+(' no shutdown\n' if up else ' shutdown\n')
 return s+routes+'line con 0\n logging synchronous\nend\n'
def lab(title,nodes,edges,notes):
 links=[];by={n['id']:n for n in nodes}
 for i,(a,pa,b,pb) in enumerate(edges):
  iface=lambda n,p:next(f['id'] for f in by[n]['interfaces'] if f['label']==p)
  links.append(dict(id='l'+str(i),n1=a,i1=iface(a,pa),n2=b,i2=iface(b,pb),label=f'{a} - {b}',conditioning={}))
 return dict(lab=dict(title=title,version='0.3.0',description='TNES - missione Network, massimo 5 nodi, rete isolata.',notes=notes),nodes=nodes,links=links,annotations=[],smart_annotations=[])
def save(name,data,mission,state):
 assert len(data['nodes'])<=5
 jsonschema.validate(data,ROOT)
 used=set()
 for l in data['links']:
  for k,j in [('n1','i1'),('n2','i2')]:assert (l[k],l[j]) not in used;used.add((l[k],l[j]))
 (OUT/(name+'.yaml')).write_text(yaml.safe_dump(data,sort_keys=False,allow_unicode=True,width=100))
 (Path('work')/(name+'.json')).write_text(json.dumps(data))
 MANIFEST.append(dict(file=name+'.yaml',mission=mission,state=state,title=data['lab']['title'],nodes=[n['label'] for n in data['nodes']],links=len(data['links'])))
base_notes='Interfacce modello 3D Gi0/x -> CML Ethernet0/x; switch Gi1/0/(x+1) -> Ethernet0/x. Console seriale esterna al conteggio nodi. Nessuna simulazione di fibra/SFP. Le password sono esempi didattici del copione, rete senza connettori esterni.'
r1=lambda:node('R1','iol-xe',router('R1',{0:('192.168.1.1','255.255.255.0',True)}),0,-180)
# Network 1: avvio senza cavi, soluzione con PC e router collegati.
ns=[node('SW1','ioll2-xe',switch('SW1'),0,0),r1(),host('PORTATILE','192.168.1.10',-240,180),host('PC2','192.168.1.11',240,180)]
edges=[('SW1','Ethernet0/0','PORTATILE','eth0'),('SW1','Ethernet0/1','PC2','eth0'),('SW1','Ethernet0/2','R1','Ethernet0/0')]
for state,ee in [('inizio',[]),('soluzione',edges)]:save('network-1-'+state,lab('TNES N1 - '+state,ns,ee,base_notes+' Inizio: collegare prima i due PC, ping e MAC; R1 solo nella fase successiva. STP senza PortFast.'),'network_1',state)
# Network 2 prima parte.
for state in ['inizio','soluzione']:
 sol=state=='soluzione';ns=[node('SW-PROVA','ioll2-xe',switch('SW-PROVA' if sol else 'Switch','192.168.1.2' if sol else None,sol),0,0),r1(),host('PORTATILE','192.168.1.10',-240,180)]
 ee=[('SW-PROVA','Ethernet0/0','PORTATILE','eth0'),('SW-PROVA','Ethernet0/1','R1','Ethernet0/0')]
 save('network-2-prova-'+state,lab('TNES N2 prova - '+state,ns,ee,base_notes+' RSA: generare le chiavi da console dopo il boot; vedere post-boot-ssh.txt. Un file startup non contiene la chiave privata.'),'network_2_prova',state)
# Network 2 sfida: 5 nodi, portatile solo console; SSH/ping da PC2.
for state in ['inizio','soluzione']:
 sol=state=='soluzione';ns=[r1(),node('SW-A','ioll2-xe',switch('SW-A' if sol else 'Switch','192.168.1.2' if sol else None,sol),-160,0),node('SW-B','ioll2-xe',switch('SW-B' if sol else 'Switch','192.168.1.3' if sol else None,sol),160,0),host('PC2','192.168.1.11',-240,180),host('PC3','192.168.1.12' if sol else '192.168.2.12',240,180)]
 ee=[('R1','Ethernet0/0','SW-A','Ethernet0/0'),('SW-A','Ethernet0/1','SW-B','Ethernet0/0'),('SW-A','Ethernet0/2','PC2','eth0'),('SW-B','Ethernet0/1','PC3','eth0')]
 save('network-2-gaia-'+state,lab('TNES N2 Gaia - '+state,ns,ee if sol else [],base_notes+' Portatile solo console. Ping/SSH da PC2. Inizio: nessun cavo, switch da configurare, PC3 su subnet errata. Soluzione: SSH richiede chiavi post-boot.'),'network_2_gaia',state)
# Network 3: esercizio guidato, soluzione e quattro guasti salvati.
for state in ['inizio','soluzione','guasti']:
 sol=state!='inizio';fault=state=='guasti'
 rr1=router('R1',{0:('192.168.1.1','255.255.255.0',True),**({1:('10.0.0.1','255.255.255.252',True)} if sol else {})},('ip route 192.168.2.0 255.255.255.0 '+('10.0.0.3' if fault else '10.0.0.2')+'\n') if sol else '')
 rr2=router('R2',{0:('10.0.0.2','255.255.255.252',True),1:('192.168.2.1','255.255.255.0',not fault)} if sol else {},'ip route 0.0.0.0 0.0.0.0 10.0.0.1\n' if sol and not fault else '')
 sw2=switch('SW2','192.168.2.2' if sol else None).replace('ip default-gateway 192.168.1.1','ip default-gateway '+('192.168.1.1' if fault else '192.168.2.1'))
 if sol:
  for device in ['R1','R2','SW2']:
   cfg={'R1':rr1,'R2':rr2,'SW2':sw2}[device].replace('end\n','enable secret Cisco123\nline con 0\n password Console123\n login\nend\n')
   if device=='R1':rr1=cfg
   elif device=='R2':rr2=cfg
   else:sw2=cfg
 ns=[node('R1','iol-xe',rr1,0,0),node('SW1','ioll2-xe',switch('SW1'),-180,0),node('R2','iol-xe',rr2,180,0),node('SW2','ioll2-xe',sw2,360,0),host('PC2','192.168.1.11',-360,0)]
 ee=[('PC2','eth0','SW1','Ethernet0/0'),('SW1','Ethernet0/1','R1','Ethernet0/0'),('R1','Ethernet0/1','R2','Ethernet0/0'),('R2','Ethernet0/1','SW2','Ethernet0/0')]
 save('network-3-'+state,lab('TNES N3 - '+state,ns,ee[:2] if state=='inizio' else ee,base_notes+' PC2 -> SW1 -> R1 -> R2 -> SW2; portatile solo console. Guasti: next-hop R1 .3, default R2 assente, Ethernet0/1 R2 shutdown, gateway SW2 fuori subnet. Il next-hop .3 è il broadcast /30 del copione: verificare accettazione su IOS reale.'),'network_3',state)
(OUT/'manifest.json').write_text(json.dumps(MANIFEST,indent=2,ensure_ascii=False))
(OUT/'post-boot-ssh.txt').write_text('''Su SW-PROVA, SW-A e SW-B nella soluzione, una volta dopo il primo avvio:\nenable\nPrivato1\nconfigure terminal\ncrypto key generate rsa general-keys modulus 2048\nend\nwrite memory\n\nDa PORTATILE (prova) o PC2 (Gaia):\nssh -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa admin@192.168.1.2\n# Password didattica: Personale1\n# Gaia: ripetere per 192.168.1.3.\n''')
print(json.dumps(MANIFEST,indent=2))
