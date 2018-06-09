package cn.buaa.act.Parser;

import cn.buaa.act.model.ActResponse;
import io.vertx.core.AsyncResult;
import io.vertx.core.Future;
import io.vertx.core.Handler;
import io.vertx.core.buffer.Buffer;
import io.vertx.core.json.DecodeException;

public class FrameParser implements Handler<Buffer> {

  private Buffer _buffer;
  private int _offset;
  private int length;
  private final Handler<AsyncResult<Object>> client;
  private ActResponse actResponse;
  public FrameParser(Handler<AsyncResult<Object>> client) {
    this.client = client;
  }
  private int state = 0;
  @Override
  public void handle(Buffer buffer) {
    append(buffer);

    //int offset;

    while (true) {
      //offset = _offset;
      int remainingBytes = bytesRemaining();
      if (state == 0 && remainingBytes >= 4) {
        int aid = _buffer.getInt(_offset);
        actResponse = new ActResponse();
        actResponse.apid = aid;
        _offset += 4;
        remainingBytes -= 4;
        state = 1;
      } else if (state == 0 &&remainingBytes < 4) {
        break;
      }
      if (state == 1 && remainingBytes >= 4) {
        length = _buffer.getInt(_offset);
        _offset += 4;
        state = 2;
        remainingBytes -= 4;
      } else if (state == 1 && remainingBytes < 4) {
        break;
      }
      if (state==2&&remainingBytes >= length) {
        actResponse.result = new String(_buffer.getBytes(_offset, _offset + length));
        _offset += length;
        state = 0;
        try {
          client.handle(Future.succeededFuture(actResponse));
        } catch (DecodeException e) {
          client.handle(Future.failedFuture(e));
        }
      } else if (state==2&&remainingBytes < length) {
        break;
      }
    }
  }

  private void append(Buffer newBuffer) {
    if (newBuffer == null) {
      return;
    }

    if (_buffer == null) {
      _buffer = newBuffer;

      return;
    }

    if (_offset >= _buffer.length()) {
      _buffer = newBuffer;
      _offset = 0;
      return;
    }

    if (_offset > 0) {
      _buffer = _buffer.getBuffer(_offset, _buffer.length());
    }
    _buffer.appendBuffer(newBuffer);
    _offset = 0;
  }

  private int bytesRemaining() {
    return (_buffer.length() - _offset) < 0 ? 0 : (_buffer.length() - _offset);
  }
}