package cn.buaa.act;

import cn.buaa.act.model.ActRequest;
import cn.buaa.act.registry.EtcdRegistry;
import cn.buaa.act.registry.IRegistry;
import cn.buaa.act.verticle.HttpServerVerticle;
import cn.buaa.act.verticle.TcpClientVerticle;
import com.coreos.jetcd.Client;
import com.coreos.jetcd.KV;
import com.coreos.jetcd.data.ByteSequence;
import com.coreos.jetcd.kv.GetResponse;
import io.vertx.core.Future;
import io.vertx.core.Vertx;
import io.vertx.core.VertxOptions;
import io.vertx.core.buffer.Buffer;
import io.vertx.core.http.HttpServer;
import io.vertx.core.http.HttpServerResponse;
import io.vertx.ext.web.Router;
import io.vertx.ext.web.handler.BodyHandler;

import java.util.Random;

/**
 * ConsumerAgent
 *
 * @author wsj
 * @date 2018/6/7
 */
public class ConsumerAgent {
    public static void main(String[] args) throws Exception {


//        System.out.println(response);

        //
//        TcpClientVerticle tcpClientVerticle = new TcpClientVerticle();
//        Vertx vertx= Vertx.vertx();
//        Random random =new Random(47);
//        vertx.deployVerticle(new HttpServerVerticle());
//        vertx.deployVerticle(tcpClientVerticle,ar -> {
//            if (ar.succeeded()) {
//                HttpServer server = vertx.createHttpServer();
//                Router router = Router.router(vertx);
//                router.route().handler(BodyHandler.create());
//                router.route().handler(routingContext -> {
//                    ActRequest actRequest = new ActRequest();
//                    Buffer buffer = Buffer.buffer();
//                    // System.out.println(actRequest.apid);
//                    buffer.appendInt(actRequest.apid);
//                    String para = routingContext.request().getFormAttribute("parameter");
//                    buffer.appendInt(para.length());
//                    buffer.appendString(para);
//                    HttpServerResponse response = routingContext.response();
//                    response.putHeader("content-type", "text/plain");
//                    tcpClientVerticle.responseMap.put(actRequest.apid,response);
//                    tcpClientVerticle.netSocketList.get(random.nextInt(3)).write(buffer);
//                });
//                server.requestHandler(router::accept).listen(20000);
//            } else {
//                Future.failedFuture(ar.cause());
//            }
//        });
        EtcdLB lb = new EtcdLB();
        Vertx vertx= Vertx.vertx();
        for(int i=0;i<8;i++){
            HttpServerVerticle httpServerVerticle = new HttpServerVerticle(lb);
            vertx.deployVerticle(httpServerVerticle,ar -> {
                if (ar.succeeded()) {

                } else {
                    Future.failedFuture(ar.cause());
                }
            });
        }
    }
}
