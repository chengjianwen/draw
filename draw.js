'use strict';
/*
  一个基于WebSocket的协作白板，支持音频
*/
function draw() {
  var ws;
  var context;
  var recorder;
  var script;
  var player;
  var startTime;
  var count;
  var canvas;
  var timer;
  var alive_id;
  var stroke;
  var sticky;
  var record;
  var brush;
  var clear;
  var user;
  var nick;
  var media;
  var last

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
    record.disabled = '';
    brush.disabled = '';
    clear.disabled = '';
    user.disabled = '';
    var msg = {
      action: 'ping'
    };
    alive_id = setInterval(() => {
      ws.send(JSON.stringify(msg));
    }, 45000);
  };

  ws.onclose = function() {
    clearInterval (alive_id);
    record.disabled = 'true';
    brush.disabled = 'true';
    clear.disabled = 'true';
    user.disabled = 'true';
  };

  ws.onerror = function() {
  };

  ws.onmessage = function(event) {
    if (event.data instanceof ArrayBuffer)
    {
      count++;
      var playTime = startTime + count * 0.512;
      var int8 = new Int8Array(event.data);
      var buffer = context.createBuffer(1, int8.length, context.sampleRate);
      for (var i = 0; i < int8.length; i++)
      {
        buffer.getChannelData(0)[i] = int8[i] / 127;
      }
      player = context.createBufferSource();
      player.buffer = buffer;
      player.connect(context.destination);
      player.start(playTime);
    }
    else
    {
      var obj = JSON.parse (event.data);
      if (obj.action == 'pong')
      { 
      } 
      else if (obj.action == 'start')
      {
        startTime = context.currentTime;
        count = 0;
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

  // 画笔选择，用户按下后，弹出选择画笔的显示区域
  brush = document.createElement("button");
  brush.innerHTML = "🖍";
//	 🖊 🖋 🖌 🖍
  brush.style.padding = '5px';
  brush.style.margin = '5px';
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
            record.disabled = '';
            brush.disabled = '';
            clear.disabled = '';
            user.disabled = '';
            available.remove();
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
    user.disabled = 'true';
  }
  sticky.appendChild(brush);

  // 清屏，用户按下后，清除屏幕内容
  clear = document.createElement("button");
  clear.innerHTML = "⎚";
  clear.style.padding = '5px';
  clear.style.margin = '5px';
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

  // 开始/停止按钮，用户按下后，输入的笔迹和声音将被发送到WebSocket服务器
  record = document.createElement("button");
  record.innerHTML = "🔇";
  record.style.padding = '5px';
  record.style.margin = '5px';
  record.onclick = start;
  sticky.appendChild(record);

  navigator.mediaDevices.getUserMedia({
    audio: true
  }).then (function(stream) {
    // 音频资源，以及音频处理器
    context = new (window.AudioContext || window.webkitAudioContext)({
      sampleRate: 16000
    });
    recorder = context.createMediaStreamSource(stream);
    script = context.createScriptProcessor(8192, 1, 1);
    script.onaudioprocess = function(e) {
      var audio = e.inputBuffer.getChannelData(0);
      var float32 = new Float32Array(audio);
      var int8 = new Int8Array(float32.length);
      for (var i = 0; i < int8.length; i++)
        int8[i] = float32[i] * 127;
      ws.send(int8.buffer);
    }
  });
  function start(e) {
    var msg = {
      action: 'start'
    };
    ws.send(JSON.stringify(msg));
    record.innerHTML = "🔊";
    recorder.connect(script);
    script.connect(context.destination);
    record.onclick = stop;
  };
// 结束录音
  function stop(e) {
    var msg = {
      action: 'stop'
    };
    ws.send(JSON.stringify(msg));
    recorder.disconnect();
    script.disconnect();
    record.innerHTML = "🔇";
    record.onclick = start;
  }

  user = document.createElement("button");
  user.innerHTML = "🧍";
  user.style.padding = '5px';
  user.style.margin = '5px';
  user.onclick = function() {
    // 显示在线用户
    var users = document.createElement("div");
    users.style.position = 'absolute';
    users.style.overflow = 'auto';
    users.style.width = '80%';
    users.style.height = '80%';
    users.style.top = '10%';
    users.style.left = '10%';
    users.style.border = '1px rgba(0, 252, 255, 0.7) solid';
    users.style.color = '#ffffff';
    users.style.background = 'rgb(6, 8, 12)';
    users.onclick = function() {
      record.disabled = '';
      brush.disabled = '';
      clear.disabled = '';
      user.disabled = '';
      users.remove();
    }

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var u = JSON.parse(xhttp.response);
        for (var i = 0; i < u.length; i++)
        {
          var div = document.createElement("div");
          div.innerHTML = u[i].nick == undefined ? 'noname' : u[i].nick;
          div.style.width = '100%';
          users.appendChild(div);
        }
      }
    };
    xhttp.open("GET", '/draw/user.php', true);
    xhttp.send();

    document.body.insertBefore(users, undefined);
    record.disabled = 'true';
    brush.disabled = 'true';
    clear.disabled = 'true';
    user.disabled = 'true';
  }
  sticky.appendChild(user);

  nick = document.createElement("input");
  nick.style.padding = '5px';
  nick.style.margin = '5px';
  nick.style.width = '3em';
  nick.oninput = function() {
    var msg = {
      action: 'nick',
      nickname: nick.value
    };
    ws.send(JSON.stringify(msg));
  }
  sticky.appendChild(nick);

  media = document.createElement("select");
  media.style.padding = '5px';
  media.style.margin = '5px';
  media.onchange = function() {
    var msg = {
      action: 'media',
      time: media.value
    };
    ws.send(JSON.stringify(msg));
  };
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var mediaes = JSON.parse(xhttp.response);
      for (var i = 0; i < mediaes.length; i++)
      {
        var option = document.createElement("option");
        option.value = mediaes[i].time;
        option.text = mediaes[i].time + '(' + mediaes[i].duration + ')';
        media.appendChild(option);
      }
    }
  };
  xhttp.open("GET", '/draw/media.php', true);
  xhttp.send();
  sticky.appendChild(media);

  timer = document.createElement("button");
  timer = document.createElement("button");
  timer.innerHTML = "0";
  timer.style.padding = '5px';
  timer.style.margin = '5px';
  timer.style.position = 'absolute';
  timer.style.right= '0';
  timer.style.top = '0';
  setInterval(() => {
    var v = timer.innerHTML.split(':');
    v[v.length - 1] = parseInt(v[v.length - 1]) + 1;
    if (v[v.length - 1] > 59)
    {
      v[v.length - 1] = '0';
      if (v.length < 2)
        v.unshift('0');
      v[v.length - 2] = parseInt(v[v.length - 2]) + 1;
      if (v[v.length - 2] > 59)
      {
        v[v.length - 2] = '0';
        if (v.length < 3)
          v.unshift('0');
        v[v.length - 3] = parseInt(v[v.length - 3]) + 1;
      }
    }
    for (var i = 0; i < v.length; i++)
    {
      v[i] = v[i].toString();
      if (v[i].length == 1)
        v[i] = '0' + v[i];
    }
    timer.innerHTML = v.join(':');
  }, 1000);

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
        ytilt: parseFloat(e.tiltY.toFixed(6)),
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

  document.body.insertBefore(timer, undefined);
  document.body.insertBefore(sticky, timer);
  document.body.insertBefore(canvas, sticky);
}
