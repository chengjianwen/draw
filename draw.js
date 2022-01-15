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

  let canvas = document.createElement("canvas");
  canvas.width = document.body.clientWidth - 10;
  canvas.height = document.body.clientHeight - 10;
  area.appendChild(canvas);

  document.body.insertBefore(area, undefined);

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
    let xhttp = new XMLHttpRequest();
    xhttp.open("POST", "/draw.fcgi", true);
    xhttp.setRequestHeader('Content-Type', 'application/json');
    xhttp.send(JSON.stringify(message));

    xhttp.onload = e => {
      JSON.parse (xhttp.response).forEach(item => {
        ctx.strokeStyle = 'rgb(' + item[2] / 256 + ',' + item[3] / 256 + ',' + item[4] / 256 + ')';
        ctx.strokeRect(item[0], item[1], 1, 1);
      });
    }
    canvas.releasePointerCapture(e.pointerId);
    canvas.onpointermove = null;
  }
  ctx = canvas.getContext('2d');

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
}
