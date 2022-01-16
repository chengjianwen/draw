function draw() {
  let sticky = document.createElement("div");
  sticky.style.position = 'sticky';
  sticky.style.height = '0px';

  start = document.createElement("button");
  start.innerHTML = "Start";
  start.style.padding = '10px 24px';
  start.style.background = "red";
  start.onclick = e => {
    start.disabled = true;
    start.style.background = "blue";
    stop.disabled = false;
    audioChunks = [];
    rec.start();
  }
  stop = document.createElement("button");
  stop.disabled = true;
  stop.innerHTML = "Stop";
  stop.style.padding = '10px 24px';
  stop.onclick = e => {
    start.disabled = false;
    stop.disabled = true;
    start.style.background = "red";
    rec.stop();
  }
  audio = document.createElement("audio");
  stop.style.padding = '10px 24px';
  audio.control = true;
  audio.autoplay = false;

  sticky.appendChild(start);
  sticky.appendChild(stop);
  sticky.appendChild(audio);
  let area = document.createElement("div");
  area.appendChild(sticky);;
  document.body.insertBefore(area, undefined);

  let canvas = document.createElement("canvas");
  canvas.width = document.body.clientWidth - 10;
  canvas.height = document.body.clientHeight - 10;
  area.appendChild(canvas);

  var ws = new WebSocket('ws://'+location.host+'/draw.ws');
  ws.onopen = function() {
    console.log('CONNECT\n'); 
    setInterval(() => {
        ws.send(String.fromCharCode(9));
	console.log('PING\n');
    }, 45000);
  };
  ws.onclose = function() {
    console.log('DISCONNECT\n'); 
  };
  ws.onmessage = function(event) {
    if (event.data.length == 0)
      console.log ('EMPTY\n');
    else if (event.data.length == 1
    && event.data == String.fromCharCode(0xa))
      console.log ('PONG\n');
    else
      JSON.parse (event.data).forEach(item => {
        ctx.strokeStyle = 'rgb(' + item[2] / 256 + ',' + item[3] / 256 + ',' + item[4] / 256 + ')';
        ctx.strokeRect(item[0], item[1], 1, 1);
      });
  };

  canvas.onpointerdown = e => {
    stroke = [];
    canvas.onpointermove = e => {
      let BB=canvas.getBoundingClientRect();
      let motion = {
        x: (e.x - BB.left) | 0,
        y: (e.y - BB.top) | 0,
        pressure: parseFloat(e.pressure.toFixed(6)),
        xtilt: parseFloat(e.tiltX.toFixed(6)),
        ytilt: parseFloat(e.tiltY.toFixed(6))
      }
      if (stroke.length == 0)
        motion.dtime = 0;
      else
        motion.dtime = new Date().getTime() - last;
      last = new Date().getTime();
      stroke.push(motion);
    }
    canvas.setPointerCapture(e.pointerId);
  }
  canvas.onpointerup = e => {
    var message = {
      width: canvas.width,
      height: canvas.height,
      stroke: stroke
    }
    ws.send(JSON.stringify(message));

    canvas.releasePointerCapture(e.pointerId);
    canvas.onpointermove = null;
  }
  ctx = canvas.getContext('2d');

/*
  let containers = {
    audio: true
  }
  navigator.mediaDevices.getUserMedia(containers)
  .then (stream => {
    rec = new MediaRecorder(stream);
    rec.ondataavailable = e => {
      audioChunks.push(e.data);
      if (rec.state == "inactive") {
        let blob = new Blob(audioChunks, {type: 'audio/mp3'});
        audio.src = URL.createObjectURL(blob);
        audio.controls  = true;
        audio.autoplay = true;
      }
    }
  });
*/
}
