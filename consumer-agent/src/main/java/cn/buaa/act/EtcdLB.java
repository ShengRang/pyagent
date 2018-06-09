package cn.buaa.act;

import com.xqbase.etcd4j.EtcdClient;
import com.xqbase.etcd4j.EtcdNode;

import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import javafx.util.Pair;

public class EtcdLB {

    public List<Pair<String, Integer>> endpoints;
    private List<Integer> rate;
    private HashMap<String, Integer> host_value;
    private Integer prev;
    private Integer maxSum;
    private Integer idx;

    public Integer size(){
        return this.endpoints.size();
    }

    EtcdLB() throws Exception {
        this.endpoints = new ArrayList<>();
        this.rate = new ArrayList<>();
        this.host_value = new HashMap<>();
        EtcdClient etcd = new EtcdClient(URI.create(System.getProperty("etcd.url")));
//        etcd.set("foo", "bar");
        List<EtcdNode> nodes = etcd.listDir("/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService");
        for(int i=0; i<nodes.size(); i++){
            EtcdNode x = nodes.get(i);
            String key = x.key;
            String value = x.value;
            int index = key.lastIndexOf("/");
            String endpointStr = key.substring(index + 1,key.length());
            String host = endpointStr.split(":")[0];
            int port = Integer.valueOf(endpointStr.split(":")[1]);
            System.out.println(host + ": " + port + "_ _ " + value);
            this.endpoints.add(new Pair<>(host, port));
            host_value.put(host, Integer.parseInt(value));
        }
        prev = 0;
        endpoints.sort((Pair<String, Integer> x, Pair<String, Integer> y) -> {
            Integer a, b;
            a = host_value.get(x.getKey()); b = host_value.get(y.getKey());
            if(a.equals(b)) {
                return 0;
            } else if (a < b){
                return 1;
            } else {
                return -1;
            }
        });
        int pre = 0;
        for(int i=0; i<endpoints.size(); i++){
            System.out.println(endpoints.get(i).getKey() + ": " + endpoints.get(i).getValue() + "  [" + host_value.get(endpoints.get(i).getKey()));
            rate.add(pre + host_value.get(endpoints.get(i).getKey()));
            pre += host_value.get(endpoints.get(i).getKey());
        }
        maxSum = pre;
        idx = 0;
        for(int i=0; i<rate.size(); i++){
            System.out.println("rate[" + i + "] = " + rate.get(i));
        }
    }

    public Pair<String, Integer> choice(){
        Pair<String, Integer> res = this.endpoints.get(idx);
        prev = (prev + 1) % maxSum;
        if(prev == 0){
            idx = 0;
        } else if(prev >= rate.get(idx)){
            idx = (idx + 1) % rate.size();
        }
        return res;
    }
}
