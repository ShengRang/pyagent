package cn.buaa.act.registry;

import java.util.List;

public interface IRegistry {

    // 注册服务
    void register(String serviceName, int port) throws Exception;

    List<Endpoint> find(String serviceName) throws Exception;
}
