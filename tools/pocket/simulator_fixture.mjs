// Only the named public test address is routed to this process's generated tone.
// The simulator's normal private-network policy remains enabled.
import http from 'node:http';
import net from 'node:net';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const [simulator, tone, mono] = process.argv.slice(2);
if (!simulator || !tone || !mono) throw new Error('Usage: node simulator_fixture.mjs <simulator-repo> <stereo.mp3> <mono.mp3>');
const stereoData=readFileSync(tone), monoData=readFileSync(mono);
const audio=http.createServer((req,res)=>{
  const data=req.url==='/chunked.mp3'?monoData:stereoData;
  if(req.url==='/invalid'){res.writeHead(200,{'Content-Type':'text/plain'});res.end('This is not MP3 audio');return;}
  console.log('Fixture request:',req.url);
  res.writeHead(200,{'Content-Type':'audio/mpeg',...(req.url==='/chunked.mp3'?{}:{'Content-Length':data.length})});
  let offset=0;
  const timer=setInterval(()=>{
    if(offset>=data.length){clearInterval(timer);res.end();return;}
    res.write(data.subarray(offset,offset+1024));offset+=1024;
  },20);
  res.on('close',()=>clearInterval(timer));
});
await new Promise(done=>audio.listen(4193,'127.0.0.1',done));
const {createAppServer}=await import(pathToFileURL(resolve(simulator,'server.mjs')));
const app=createAppServer({allowLocalFirmwareUpload:true,networkBridge:{createTcpConnection(options){
  return net.createConnection(options.host==='93.184.216.34' && options.port===80
    ? {...options,host:'127.0.0.1',port:4193}:options);
}}});
app.listen(4190,'127.0.0.1',()=>console.log('Simulator + isolated MP3 fixtures: http://127.0.0.1:4190'));
