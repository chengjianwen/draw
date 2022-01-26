'use strict';
/*
  一个基于WebSocket的协作白板，支持音频
*/
function draw() {

  var ws;
  var context;
  var canvas;
  var timer_id;
  var alive_id;
  var stroke;
  var sticky;
  var record;
  var brush;
  var clear;

  let wsurl;
  if (location.protocol == 'http:')
    wsurl = 'ws://';
  else if (location.protocol == 'https:')
    wsurl = 'wss://';
  wsurl += location.host + '/draw.ws';

  ws = new WebSocket(wsurl);
  ws.binaryType = "arraybuffer";

  // 每间隔45秒，发送一个ping，以保持连接不被服务器关闭
  ws.onopen = function() {
    record.disable = 'false';
    brush.disable = 'false';
    clear.disable = 'false';
    var msg = {
      action: 'ping'
    };
    alive_id = setInterval(() => {
      ws.send(JSON.stringify(msg));
      console.log('PING\n');
    }, 45000);
  };

  ws.onclose = function() {
    clearInterval (alive_id);
    record.disable = 'true';
    brush.disable = 'true';
    clear.disable = 'true';
  };

  ws.onerror = function() {
  };

  ws.onmessage = function(event) {
    if (event.data instanceof ArrayBuffer)
    {
      var float32 = new Float32Array(event.data);
      var buffer = context.createBuffer(1, float32.length, context.sampleRate);
      for (var i = 0; i < float32.length; i++)
      {
        buffer.getChannelData(0)[i] = float32[i];
      }
      var player = context.createBufferSource();
      player.connect(context.destination);
      player.buffer = buffer;
      player.start();
    }
    else
    {
      var obj = JSON.parse (event.data);
      if (obj.action == 'pong')
      { 
      } 
      else if (obj.action == 'clear')
      {
        canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
      }
      else // drawing
      {
        var radioWidth  = obj.width / canvas.width;
        var radioHeight = obj.height / canvas.height;
        var radio       = Math.max(radioWidth, radioHeight);
        var ctx         = canvas.getContext('2d');
        ctx.lineWidth   = 0.1;
        for (var i = 0; i < obj.stroke.length; i++)
        {
          var x           = obj.stroke[i][0];
          var y           = obj.stroke[i][1];
          var r           = obj.stroke[i][2];
          var g           = obj.stroke[i][3];
          var b           = obj.stroke[i][4];
          ctx.strokeStyle = 'rgb(' + r / 256 + ',' + g / 256 + ',' + b / 256 + ')';
          ctx.strokeRect(x / radio, y / radio, 0.1, 0.1);
        }
      }
    }
  };

  // 一个用于放置操作按钮的位置固定的区域
  sticky = document.createElement("div");
  sticky.style.position = 'absolute';
  sticky.style.left = '0';
  sticky.style.top = '0';

  // 开始/停止按钮，用户按下后，输入的笔迹和声音将被发送到WebSocket服务器
  record = document.createElement("button");
  record.innerHTML = "●";
  record.style.padding = '5px';
  record.style.margin = '5px';
  record.style.fontSize = 'x-large';
  record.onclick = start;
  sticky.appendChild(record);

  function start(e) {
    record.innerHTML = "00";
    navigator.mediaDevices.getUserMedia({
      audio: true
    }).then (function(stream) {
      // 音频资源，以及音频处理器
      context = new AudioContext();
      var recorder = context.createMediaStreamSource(stream);
      var processor = context.createScriptProcessor(4096, 1, 1);
      recorder.connect(processor);
      processor.connect(context.destination);
      processor.onaudioprocess = function(e) {
        ws.send(e.inputBuffer.getChannelData(0));
      }
    });
    timer_id = setInterval(() => {
      var v = record.innerHTML.split(':');
      v[v.length - 1] = parseInt(v[v.length - 1]) + 1;
      if (v[v.length - 1] > 59)
      {
        v[v.length - 1] = '00';
        if (v.length < 2)
          v.unshift('00');
        v[v.length - 2] = parseInt(v[v.length - 2]) + 1;
        if (v[v.length - 2] > 59)
        {
          v[v.length - 2] = '00';
          if (v.length < 3)
            v.unshift('00');
          v[v.length - 3] = parseInt(v[v.length - 3]) + 1;
        }
      }
      for (var i = 0; i < v.length; i++)
      {
        v[i] = v[i].toString();
        if (v[i].length == 1)
          v[i] = '0' + v[i];
      }
      record.innerHTML = v.join(':');
    }, 1000);
    record.onclick = stop;
  };

  // 结束录音
  function stop(e) {
    clearInterval (timer_id);
    record.innerHTML = "●";
    context.close();
    record.onclick = start;
  }

  // 画笔选择，用户按下后，弹出选择画笔的显示区域
  brush = document.createElement("button");
  brush.innerHTML = "🖌️";
  brush.style.padding = '5px';
  brush.style.margin = '5px';
  brush.style.fontSize = 'x-large';
  brush.onclick = function() {
    // 画笔选择区域，显示可用的画笔
    var available = document.createElement("div");
    available.style.position = 'absolute';
    available.style.overflow = 'auto';
    available.style.width = '80%';
    available.style.height = '80%';
    available.style.top = '10%';
    available.style.left = '10%';
    available.style.border = '1px rgba(0, 252, 255, 0.7) solid';
    available.style.color = '#ffffff';
    available.style.background = 'rgb(6, 8, 12)';

    var search = document.createElement("input");
    search.type = 'search';
    search.style.width = '100%';
    search.oninput = function() {
      var children = available.children;
      for (var i = 1; i < children.length; i++)
        children[i].hidden = children[i].src.search(search.value) < 0;
    }
    available.appendChild(search);

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var brushes = JSON.parse(xhttp.response);
        for (var i = 0; i < brushes.length; i++)
        {
          var img = new Image();
          img.src = '/brushes/' + brushes[i];
          img.style.width = '25%';
          img.onclick = function(e) {
            available.remove();
            record.disable = 'false';
            brush.disable = 'false';
            clear.disable = 'false';
            xhttp.onreadystatechange = function() {
              if (this.readyState == 4 && this.status == 200) {
                var msg = {
                  action: 'brush',
                  brush: JSON.parse(xhttp.response)
                };
                ws.send(JSON.stringify(msg));
              }
            }
            xhttp.open("GET", e.target.src.replace(/_prev.png/i, '.myb'), true);
            xhttp.send();
          }
          available.appendChild(img);
        }
      }
    };
    xhttp.open("GET", '/brushes/brushes.json', true);
    xhttp.send();

    document.body.insertBefore(available, undefined);
    record.disabled = 'true';
    brush.disabled = 'true';
    clear.disabled = 'true';
  }
  sticky.appendChild(brush);

  // 清屏，用户按下后，清除屏幕内容
  clear = document.createElement("button");
  clear.innerHTML = "⎚";
  clear.style.padding = '5px';
  clear.style.margin = '5px';
  clear.style.fontSize = 'x-large';
  clear.onclick = function() {
    canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
    if (ws.readyState == WebSocket.OPEN)
    {
      var msg = {
        action: 'clear'
      }
      ws.send(JSON.stringify(msg));
    }
  }
  sticky.appendChild(clear);

  // 绘制区
  canvas = document.createElement("canvas");
  canvas.width = document.body.offsetWidth;
  canvas.height = document.body.offsetHeight;
  canvas.onpointerdown = function() {
    stroke = new Array();
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
      var last = new Date().getTime();
      stroke.push(motion);
    }
  };

  // 当绘笔抬起时，将收集到的绘笔的移动轨迹信息发送给服务器
  canvas.onpointerup = function() {
    for (var i = 0; i < stroke.length; i++)
    {
      stroke[i].x = stroke[i].x * 10;
      stroke[i].y = stroke[i].y * 10;
    }
    var msg = {
      action: 'stroke',
      width: canvas.width * 10,
      height: canvas.height * 10,
      stroke: stroke
    };
    ws.send(JSON.stringify(msg));
    canvas.onpointermove = null;
  }

  document.body.insertBefore(sticky, undefined);
  document.body.insertBefore(canvas, sticky);
}
