# Builder container
FROM registry.cn-hangzhou.aliyuncs.com/aliware2018/services AS builder

COPY . /root/dists
WORKDIR /root/dists/consumer-agent
RUN set -ex && mvn clean package


# Runner container
FROM pypy:2

RUN git clone https://github.com/libuv/libuv.git \
    && cd libuv \
    && sh autogen.sh \
    && ./configure \
    && make \
#    && make check \
    && make install \
    && ldconfig \
    && cd ..

ARG DEBIAN_FRONTEND=noninteractive
ARG JAVA_VERSION=8
ARG JAVA_UPDATE=172
ARG JAVA_BUILD=11
ARG JAVA_PACKAGE=jdk
ARG JAVA_HASH=a58eab1ec242421181065cdc37240b08

ENV LANG C.UTF-8
ENV JAVA_HOME=/opt/jdk
ENV PATH=${PATH}:${JAVA_HOME}/bin

RUN set -ex \
 && apt-get update \
 && apt-get install -y ca-certificates curl gnupg net-tools procps unzip \
 && curl -L --header "Cookie: oraclelicense=accept-securebackup-cookie" \
         -o /tmp/java.tar.gz \
         http://download.oracle.com/otn-pub/java/jdk/${JAVA_VERSION}u${JAVA_UPDATE}-b${JAVA_BUILD}/${JAVA_HASH}/${JAVA_PACKAGE}-${JAVA_VERSION}u${JAVA_UPDATE}-linux-x64.tar.gz \
 && CHECKSUM=$(curl -L https://www.oracle.com/webfolder/s/digest/${JAVA_VERSION}u${JAVA_UPDATE}checksum.html | grep -E "${JAVA_PACKAGE}-${JAVA_VERSION}u${JAVA_UPDATE}-linux-x64\.tar\.gz" | grep -Eo '(sha256: )[^<]+' | cut -d: -f2 | xargs) \
 && echo "${CHECKSUM}  /tmp/java.tar.gz" | sha256sum -c \
 && mkdir ${JAVA_HOME} \
 && tar -xzf /tmp/java.tar.gz -C ${JAVA_HOME} --strip-components=1 \
 && curl -L --header "Cookie: oraclelicense=accept-securebackup-cookie;" \
         -o /tmp/jce_policy.zip \
         http://download.oracle.com/otn-pub/java/jce/${JAVA_VERSION}/jce_policy-${JAVA_VERSION}.zip \
 && unzip -jo -d ${JAVA_HOME}/jre/lib/security /tmp/jce_policy.zip \
 && rm -rf ${JAVA_HOME}/jar/lib/security/README.txt \
       /var/lib/apt/lists/* \
       /tmp/*
COPY --from=builder /root/dists/requirements.txt ./
RUN pypy -m pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
#RUN pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple

COPY --from=builder /root/workspace/services/mesh-provider/target/mesh-provider-1.0-SNAPSHOT.jar /root/dists/mesh-provider.jar
COPY --from=builder /root/workspace/services/mesh-consumer/target/mesh-consumer-1.0-SNAPSHOT.jar /root/dists/mesh-consumer.jar
COPY --from=builder /root/dists/*.py /root/dists/
COPY --from=builder /root/dists/consumer-agent/target/consumer-agent-1.0-SNAPSHOT.jar /root/dists/ca.jar

COPY --from=builder /usr/local/bin/docker-entrypoint.sh /usr/local/bin

COPY --from=builder /root/dists/uv/* /root/dists/uv/

COPY start-agent.sh /usr/local/bin

RUN apt-get update && apt-get install -y llvm clang

RUN set -ex \
 && chmod a+x /usr/local/bin/start-agent.sh \
 && mkdir -p /root/logs

WORKDIR /root/dists/uv
RUN clang++ -fpermissive -O3 pa.cc utils.cc dubbo_client.cc bytebuf.cc -luv -o /root/dists/pa.out
RUN clang++ -O3 ca.cc log.cc utils.cc pa_client.cc bytebuf.cc http-parser/http_parser.c  -luv -o /root/dists/ca.out

EXPOSE 8087

ENTRYPOINT ["docker-entrypoint.sh"]
