function draw() {
  // 一个用于放置操作按钮的位置固定的区域
  let sticky = document.createElement("div");
  sticky.style.position = 'sticky';
  sticky.style.height = '0px';

  // 开始按钮，用户按下后，开始记录输入的笔迹和声音
  start = document.createElement("button");
  start.innerHTML = "开始";
  start.style.padding = '10px 24px';
  start.style.background = "red";
  start.onclick = function() {
    start.disabled = true;
    start.style.background = "blue";
    stop.disabled = false;
    context.resume();
  };
  sticky.appendChild(start);

  // 结束按钮，用户按下后，停止记录输入的笔迹和声音
  stop = document.createElement("button");
  stop.disabled = true;
  stop.innerHTML = "停止";
  stop.style.padding = '10px 24px';
  stop.onclick = function() {
    start.disabled = false;
    stop.disabled = true;
    start.style.background = "red";
    context.suspend();
  }
  sticky.appendChild(stop);

  // 画笔选择，用户按下后，弹出选择画笔的对话框
  brush = document.createElement("button");
  brush.innerHTML = "画笔";
  brush.style.padding = '10px 24px';
  brush.onclick = function() {
    // 画笔选择区域，显示可用的画笔
    brush_area = document.createElement("div");
    brush_area.innerHTML = "brush list";
    brush_area.style.position = 'absolute';
    brush_area.style.overflow = 'auto';
    brush_area.style.width = '80%';
    brush_area.style.height = '80%';
    brush_area.style.top = '10%';
    brush_area.style.left = '10%';
    brush_area.style.border = '1px rgba(0, 252, 255, 0.7) solid';
    brush_area.style.color = '#ffffff';
    brush_area.style.background = 'rgb(6, 8, 12)';

    brush_area.onclick = function(e) {
      while (e.target.firstChild)
        e.target.removeChild(e.target.firstChild);
      e.target.remove();
    }
    brush_area.style.cursor = 'pointer';
    document.body.insertBefore(brush_area, undefined);

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var brushes = JSON.parse(xhttp.response);
        for (var i = 0; i < brushes.length; i++)
        {
          o = document.createElement("div");
          o.innerHTML = brushes[i];
          o.onclick = function(e) {
            e.target.parentElement.remove();
            xhttp.onreadystatechange = function() {
              if (this.readyState == 4 && this.status == 200) {
                var msg = {
                  action: 'brush',
                  brush: JSON.parse(xhttp.response)
                };
                ws.send(JSON.stringify(msg));
              }
            }
            url = location.protocol + '//' + location.host + '/brushes/' + e.target.innerHTML;
            url = url.replace('_prev.png', '.myb');
            xhttp.open("GET", url, true);
            xhttp.send();
          }
          brush_area.appendChild(o);
        }
      }
    };
    url = location.protocol + '//' + location.host + '/brushes/brushes.json';
    xhttp.open("GET", url, true);
    xhttp.send();
  }
  sticky.appendChild(brush);

  // 清除屏幕，用户按下后，清除屏幕内容
  clear = document.createElement("button");
  clear.innerHTML = "清除";
  clear.style.padding = '10px 24px';
  clear.onclick = function() {
    var msg = {
      action: 'clear'
    }
    ws.send(JSON.stringify(msg));
    canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
  }
  sticky.appendChild(clear);

  // 声音信号采集器
  navigator.mediaDevices.getUserMedia({
    audio: true
  }).then (stream => {
    context = new window.AudioContext();
    recorder = context.createScriptProcessor(4096, 1, 1);
    source = context.createMediaStreamSource(stream);
    source.connect(recorder);
    recorder.connect(context.destination);
    recorder.onaudioprocess = function(e) {
      var left = e.inputBuffer.getChannelData(0);
      ws.send(left);
    }
    context.suspend();
  });

  // 界面区域
  area = document.createElement("div");
  area.appendChild(sticky);

  // 绘制区
  canvas = document.createElement("canvas");
  canvas.width = document.body.clientWidth;
  canvas.height = document.body.clientHeight;
  canvas.onpointerdown = function() {
    stroke = [];
    canvas.onpointermove = function(e) {
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
  };

  // 当绘笔抬起时，将收集到的绘笔的移动轨迹信息发送给服务器
  canvas.onpointerup = function() {
    var msg = {
      action: 'stroke',
      width: canvas.width,
      height: canvas.height,
      stroke: stroke
    };
    ws.send(JSON.stringify(msg));

    canvas.onpointermove = null;
  }
  area.appendChild(canvas);

  document.body.insertBefore(area, undefined);

  // 通过location解析得到WebSockets URL
  var url;
  if (location.protocol == 'http:')
    url = 'ws:';
  else if (location.protocol == 'https:')
    url = 'wss:';
  url += '//';
  url += location.host;
  url += '/draw.ws';

  // 创建WebSocket
  var ws = new WebSocket(url);
  ws.binaryType = "arraybuffer";

  // 每间隔45秒，发送一个ping连接
  ws.onopen = function() {
    console.log('CONNECT\n'); 
    var msg = {
      action: 'ping'
    };
    ping_id = setInterval(() => {
      ws.send(JSON.stringify(msg));
      console.log('PING\n');
    }, 45000);
  };

  ws.onclose = function() {
    clearInterval (ping_id);
    console.log('DISCONNECT\n'); 
  };

  ws.onerror = function() {
    console.log('ERROR: \n'); 
  };

  ws.onmessage = function(event) {
    if (event.data instanceof ArrayBuffer)
    {
      float32 = new Float32Array(event.data);
      var buffer = context.createBuffer(2, float32.length, context.sampleRate);
      for (var i = 0; i < float32.length; i++)
      {
        buffer.getChannelData(0)[i] = float32[i];
      }
      source = context.createBufferSource();
      source.buffer = buffer;
      source.connect(context.destination);
      source.start();
    }
    else
    {
      var obj = JSON.parse (event.data);
      if (obj.action == 'pong')
      { 
        console.log('PONG');
      } 
      else if (obj.action == 'clear')
      {
        canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
      }
      else
      { 
        obj.stroke.forEach(item => {
          canvas.getContext('2d').strokeStyle = 'rgb(' + item[2] / 256 + ',' + item[3] / 256 + ',' + item[4] / 256 + ')';
          var radio = obj.width / canvas.width > obj.height / canvas.height ? obj.width / canvas.width :  obj.height / canvas.height;
          canvas.getContext('2d').strokeRect(item[0] / radio, item[1] / radio, 1, 1);
        });
      }
    }
  };
}
