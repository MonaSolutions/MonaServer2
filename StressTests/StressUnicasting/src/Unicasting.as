package
{
	import flash.events.NetStatusEvent;
	import flash.net.NetConnection;
	import flash.net.NetStream;
	import flash.utils.ByteArray;

	public class Unicasting
	{
		
		public static const STATE_CONNECTED:uint = 0;
		public static const STATE_REJECTED:uint = 1;
		public static const STATE_WAITING:uint = 3;
		public static const STATE_PLAYING:uint = 4;

		private var _connection:NetConnection = new NetConnection();
		private var _stream:NetStream;
		private var _handler:Object;
		private var _name:String = "test";
		private var _state:uint = STATE_REJECTED;
		
		private function set state(value:uint):void {
			if(value==_state)
				return;
			_handler.onChange(_state,value);
			_state = value;
		}
		
		


		public function Unicasting(handler:Object,url:String) {
			_connection.addEventListener(NetStatusEvent.NET_STATUS,onStatus,false,0,true);
			_connection.client = this;
			_handler=handler;
			var found:int = url.lastIndexOf("/");
			if(found>=0) {
				_name = url.substr(found+1);
				url = url.substring(0,found);
			}
			_name += "?onAudio=onAudio&onVideo=onVideo";
			_connection.connect(url);
		}
		
		public function onBWDone():void {
			
		}
		public function onVideo(time:uint,data:ByteArray):void {
			
		}
		public function onAudio(time:uint,data:ByteArray):void {
			
		}
		
		public function onMetaData(data:Object):void {
			
		}
		
		public function onTextData(subtitle:Object):void {
	
		}
		
		public function close():void {
			_connection.close();
			if(_stream)
				_stream.close();
		}
		
		private function onStatus(e:NetStatusEvent):void {
			trace("Net Status Code: " + e.info.code);
			switch(e.info.code) {
				
				case "NetConnection.Connect.Success":
					state = STATE_CONNECTED;
					_stream = new NetStream(_connection);
					_stream.client = this;
					_stream.addEventListener(NetStatusEvent.NET_STATUS,onStatus,false,0,true);
					_stream.play(_name);
					break;
				
				case "NetConnection.Connect.Closed":
					state = STATE_REJECTED;
					close();
					break;
				case "NetStream.Play.Start":
					if(_state!=STATE_PLAYING)
						state = STATE_WAITING;
					break;
				case "NetStream.Play.PublishNotify":
					state = STATE_PLAYING;
					break;
				case "NetStream.Play.UnpublishNotify":
					state = STATE_WAITING;
					break;
				default:
					break;
			}
			if(e.info.level=="error") {
				state = STATE_REJECTED;
				close();
			}
		}
	}
}