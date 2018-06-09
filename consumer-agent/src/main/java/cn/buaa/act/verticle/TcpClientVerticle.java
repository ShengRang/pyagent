package cn.buaa.act.verticle;

import cn.buaa.act.Parser.FrameParser;
import cn.buaa.act.model.ActResponse;
import cn.buaa.act.registry.Endpoint;
import cn.buaa.act.registry.IRegistry;
import io.vertx.core.AbstractVerticle;
import io.vertx.core.Future;
import io.vertx.core.buffer.Buffer;
import io.vertx.core.http.HttpServerResponse;
import io.vertx.core.net.NetClient;
import io.vertx.core.net.NetClientOptions;
import io.vertx.core.net.NetSocket;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * TcpClientVerticle
 *
 * @author wsj
 * @date 2018/6/7
 */
public class TcpClientVerticle extends AbstractVerticle {
//    private IRegistry iRegistry;
//    public List<Endpoint> endpoints = null;
//    public Map<String,NetSocket> netSocketMap =new HashMap<>();
//    public List<NetSocket> netSocketList =new ArrayList<>();
//
//
//    public String name;
//    public TcpClientVerticle(IRegistry iRegistry){
//        this.iRegistry = iRegistry;
//        try {
//            if (null == endpoints) {
//                endpoints = iRegistry.find("com.alibaba.dubbo.performance.demo.provider.IHelloService");
//            }
//        } catch (Exception e) {
//            System.out.println("shit ri fuck mabi get endpoints fail");
//            e.printStackTrace();
//        }
//    }
    private static final Logger logger = LoggerFactory.getLogger(TcpClientVerticle.class);
    public TcpClientVerticle(){}

    public NetSocket netSocket;
    private int port;
    private String host;
    private int size;
    public ConcurrentHashMap<Integer,HttpServerResponse> responseMap =new ConcurrentHashMap<>();
    public TcpClientVerticle(int port,String host,int size){
        this.port=port;
        this.host=host;
        this.size=size;
    }
    @Override
    public void start(Future<Void> startFuture) throws Exception {
        for(int i = 0; i < size; i++){
            createClient(new NetClientOptions(),port,host)
                    .setHandler(ar -> {
                        if (ar.failed()) {
                            Throwable cause = ar.cause();
                            Future.failedFuture(cause);
                        }
                    });
        }
        startFuture.complete();
    }
    private Future<Void> createClient(NetClientOptions options,int port,String host) {
        Future<Void> createClientFuture = Future.future();
        NetClient client = vertx.createNetClient(options);
        client.connect(port,host,res -> {
            if (res.succeeded()) {
                System.out.println(host+"Connected!");
                NetSocket socket = res.result();
                netSocket=socket;
                //netSocketList.add(socket);
//                socket.handler(buffer -> {
//                    getHashResult(buffer)
//                            .compose(response->{
//                                return writeHttpResponse(response);
//                            })
//                            .setHandler(ar3 -> {
//                                if (ar3.succeeded()) {
//
//                                }
//                                else {
//                                    Throwable cause = ar3.cause();
//                                    Future.failedFuture(cause);
//                                }
//                            });
                socket.handler(new FrameParser(fres -> {
                    if (fres.failed()) {
                        System.out.println(fres.cause());
                        return;
                    }
                    else {
                        ActResponse object = (ActResponse)fres.result();
                        writeHttpResponse(object);
                    }
                }));

            } else {
                System.out.println("Failed to connect: " + res.cause().getMessage());
            }
        });
        createClientFuture.complete();
        return createClientFuture;
    }
    private Future<Void> writeHttpResponse(ActResponse actResponse) {
        Future<Void> fu = Future.future();
        String [] resultArray= actResponse.result.split("\n");
        HttpServerResponse httpServerResponse= responseMap.get(actResponse.apid);
        httpServerResponse.end(String.valueOf(resultArray[1]));
        //responseMap.remove(actResponse.apid);
        fu.complete();
        return fu;
    }

//    private Future<ActResponse> getHashResult(Buffer buffer){
//        Future<ActResponse> fu = Future.future();
//        ActResponse actResponse =new ActResponse();
//        String bufferStr = buffer.toString();
//        //System.out.println(bufferStr);
//        fu.complete(actResponse);
//        return fu;
//    }
}
