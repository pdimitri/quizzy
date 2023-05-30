  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);


  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function onOpen(event) {
    console.log('Connection opened');
    getValues();
  }

  function getValues(){
    websocket.send("getValues");
  }

  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }

  function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        
        if (key=="GPIO_State"){
          if(myObj[key]==1){
            document.getElementById("left-section").className = "left-section-red";
          }
          if(myObj[key]==2){
            document.getElementById("right-section").className = "right-section-yellow";
          }
          if(myObj[key]==0){
            document.getElementById("right-section").className = "right-section";
            document.getElementById("left-section").className = "left-section";
          }
        } else{
          document.getElementById(key).innerHTML = myObj[key];
        }
    }
  }

  function onLoad(event) {
    initWebSocket();
  }
  