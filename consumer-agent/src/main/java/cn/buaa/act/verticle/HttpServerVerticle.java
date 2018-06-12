package cn.buaa.act.verticle;

import cn.buaa.act.EtcdLB;
import cn.buaa.act.model.ActRequest;
import cn.buaa.act.registry.Endpoint;
import cn.buaa.act.registry.IRegistry;
import io.vertx.core.AbstractVerticle;
import io.vertx.core.Future;
import io.vertx.core.buffer.Buffer;
import io.vertx.core.http.HttpServer;
import io.vertx.core.http.HttpServerResponse;
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
    public HttpServerVerticle(){}
    private Future<Void> createServer(){
        Future<Void> createServerFuture = Future.future();
        HttpServer server = vertx.createHttpServer();
        Router router = Router.router(vertx);
        router.route().handler(BodyHandler.create());
        //!!!!随机数
        Map<String,TcpClientVerticle> paMap=new HashMap<>();
        Map<String,Boolean> paFirst = new HashMap<>();
        if(paMap.size()==0){
            for(int i=0;i<lb.endpoints.size();i++){

//                Endpoint ep = endpoints.get(0);
                final String key = lb.endpoints.get(i).getKey();
                TcpClientVerticle tcpClientVerticle =new TcpClientVerticle(lb.endpoints.get(i).getValue(),key,1);
                vertx.deployVerticle(tcpClientVerticle,ar -> {
                    if (ar.succeeded()) {
                        paMap.put(key,tcpClientVerticle);
                        paFirst.put(key,true);
                    } else {
                        Future.failedFuture(ar.cause());
                    }
                });
            }
        }
        router.route().handler(routingContext -> {
                ActRequest actRequest = new ActRequest();

                // System.out.println(actRequest.apid);
                actRequest.Interface = routingContext.request().getFormAttribute("interface");
                actRequest.Method = routingContext.request().getFormAttribute("method");
                actRequest.ParameterTypesString = routingContext.request().getFormAttribute("parameterTypesString");

                Buffer buffer = Buffer.buffer();
                buffer.appendInt(actRequest.apid);
                String para = routingContext.request().getFormAttribute("parameter");
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
//                    header.appendBuffer(buffer);
                    paMap.get(select).responseMap.put(actRequest.apid,response);
                    paMap.get(select).netSocket.write(header);
                    paMap.get(select).netSocket.write(buffer);
                }
                else {
                    paMap.get(select).responseMap.put(actRequest.apid,response);
//                    System.out.println("write act request: " + actRequest.apid + " ");
                    paMap.get(select).netSocket.write(buffer);
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
