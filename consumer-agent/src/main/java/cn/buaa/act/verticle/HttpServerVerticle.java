package cn.buaa.act.verticle;

import cn.buaa.act.EtcdLB;
import cn.buaa.act.Parser.FrameParser;
import cn.buaa.act.model.ActRequest;
import cn.buaa.act.model.ActResponse;
import cn.buaa.act.registry.Endpoint;
import cn.buaa.act.registry.IRegistry;
import io.vertx.core.AbstractVerticle;
import io.vertx.core.Future;
import io.vertx.core.buffer.Buffer;
import io.vertx.core.http.HttpServer;
import io.vertx.core.http.HttpServerResponse;
import io.vertx.core.net.NetClient;
import io.vertx.core.net.NetClientOptions;
import io.vertx.core.net.NetSocket;
import io.vertx.ext.web.Router;
import io.vertx.ext.web.handler.BodyHandler;
import javafx.util.Pair;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

/**
 * HttpServerVerticle
 *
 * @author wsj
 * @date 2018/6/7
 */
public class HttpServerVerticle extends AbstractVerticle {
    EtcdLB lb;
    private static final Logger logger = LoggerFactory.getLogger(HttpServerVerticle.class);
    @Override
    public void start(Future<Void> startFuture) throws Exception {
        for(int i = 0; i < 1; i++){
            createServer().setHandler(ar -> {
                        if (ar.failed()) {
                            Throwable cause = ar.cause();
                            Future.failedFuture(cause);
                        }
                    });
        }
    }
    public HttpServerVerticle(EtcdLB lb){
        this.lb = lb;
    }
    public HttpServerVerticle() throws Exception{
        this.lb = new EtcdLB();
    }


    public Map<Integer,HttpServerResponse> responseMap =new HashMap<>();
    private void writeHttpResponse(ActResponse actResponse) {
        StringTokenizer st = new StringTokenizer(actResponse.result);
        st.nextToken();
        responseMap.get(actResponse.apid).end(st.nextToken());
        responseMap.remove(actResponse.apid);
    }
    private Future<NetSocket> createClient(NetClientOptions options, int port, String host) {
        Future<NetSocket> createClientFuture = Future.future();
        NetClient client = vertx.createNetClient(options);
        client.connect(port,host,res -> {
            if (res.succeeded()) {
                System.out.println(host+"Connected!");
                NetSocket socket = res.result();
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
                createClientFuture.complete(socket);
            } else {
                System.out.println("Failed to connect: " + res.cause().getMessage());
            }
        });
        return createClientFuture;
    }

    private Future<Void> createServer(){
        Future<Void> createServerFuture = Future.future();
        HttpServer server = vertx.createHttpServer();
        Router router = Router.router(vertx);
        router.route().handler(BodyHandler.create());
        //!!!!随机数
        Map<String,NetSocket> paMap=new HashMap<>();
        Map<String,Boolean> paFirst = new HashMap<>();
        if(paMap.size()==0){
            for(int i=0;i<lb.endpoints.size();i++){

                final String key = lb.endpoints.get(i).getKey();
                //TcpClientVerticle tcpClientVerticle =new TcpClientVerticle(lb.endpoints.get(i).getValue(),key,1);
//                vertx.deployVerticle(tcpClientVerticle,ar -> {
//                    if (ar.succeeded()) {
//                        paMap.put(key,tcpClientVerticle);
//                        paFirst.put(key,true);
//                    } else {
//                        Future.failedFuture(ar.cause());
//                    }
//                });
                createClient(new NetClientOptions(),lb.endpoints.get(i).getValue(),key).setHandler(ar -> {
                    if (ar.failed()) {
                        Throwable cause = ar.cause();
                        Future.failedFuture(cause);
                    }
                    else {
                        paMap.put(key,ar.result());
                        paFirst.put(key,true);
                    }
                });
            }
        }
        ActRequest actRequest = new ActRequest();
        Random random1= new Random(Thread.currentThread().getId());
        router.route().handler(routingContext -> {
                actRequest.apid = random1.nextInt();
                if (actRequest.apid < 0) {
                    actRequest.apid = - actRequest.apid;
                }
                 // System.out.println(actRequest.apid);
                actRequest.Interface = routingContext.request().getFormAttribute("interface");
                actRequest.Method = routingContext.request().getFormAttribute("method");
                actRequest.ParameterTypesString = routingContext.request().getFormAttribute("parameterTypesString");


                String para = routingContext.request().getFormAttribute("parameter");
                Buffer buffer = Buffer.buffer(8+para.length());
                buffer.appendInt(actRequest.apid);
                buffer.appendInt(para.length());
                buffer.appendString(para);
                HttpServerResponse response = routingContext.response();
                response.putHeader("content-type", "text/plain");
                Pair<String, Integer> choice = lb.choice();

                String select = choice.getKey();

                // TcpClientVerticle tcpClientVerticle;
                if(paFirst.get(select)){
                    paFirst.put(select,false);
                    Buffer header =Buffer.buffer();
                    header.appendBytes(new byte[]{(byte) 0xab,(byte)0xcd});
                    header.appendInt(actRequest.Interface.length()).appendString(actRequest.Interface);
                    header.appendInt(actRequest.Method.length()).appendString(actRequest.Method);
                    header.appendInt(actRequest.ParameterTypesString.length()).appendString(actRequest.ParameterTypesString);
                    responseMap.put(actRequest.apid,response);
                    paMap.get(select).write(header);
                    paMap.get(select).write(buffer);
                }
                else {
                    responseMap.put(actRequest.apid,response);
                    paMap.get(select).write(buffer);
                }
            //tcpClientVerticle.responseMap.put(actRequest.apid,response);
            //tcpClientVerticle.netSocketList.get(random.nextInt(3)).write(buffer);
        });
        server.requestHandler(router::accept).listen(20000,"localhost", res -> {
            if (res.succeeded()) {
                System.out.println("Http Server is now listening!");
            } else {
                System.out.println("Failed to bind!");
            }
        });
        createServerFuture.complete();
        return createServerFuture;
    }

}
