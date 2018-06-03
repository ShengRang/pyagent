# Builder container
FROM registry.cn-hangzhou.aliyuncs.com/aliware2018/services AS builder

COPY . /root/dists
WORKDIR /root/dists
#RUN set -ex && mvn clean package


# Runner container
FROM pypy:2

RUN apt-get update && apt-get install openjdk-8-jdk

RUN pypy -m pip install tornado

COPY --from=builder /root/workspace/services/mesh-provider/target/mesh-provider-1.0-SNAPSHOT.jar /root/dists/mesh-provider.jar
COPY --from=builder /root/workspace/services/mesh-consumer/target/mesh-consumer-1.0-SNAPSHOT.jar /root/dists/mesh-consumer.jar
COPY --from=builder /root/dists/main.py /root/dists/main.py
COPY --from=builder /root/dists/pa.py /root/dists/pa.py

COPY --from=builder /usr/local/bin/docker-entrypoint.sh /usr/local/bin
COPY --from=builder /root/dists/docker-entrypoint.sh /usr/local/bin

COPY start-agent.sh /usr/local/bin

RUN set -ex \
 && chmod a+x /usr/local/bin/start-agent.sh \
 && mkdir -p /root/logs

EXPOSE 8087

ENTRYPOINT ["docker-entrypoint.sh"]
