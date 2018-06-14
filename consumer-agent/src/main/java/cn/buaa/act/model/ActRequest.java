package cn.buaa.act.model;

import java.util.concurrent.atomic.AtomicInteger;

public class ActRequest {
    public int apid;                       // 用于识别请求的 id
    public String Interface;             // 这个是用来识别哪个 end point 的 TODO: 这个字段是必须的
    public String Method;
    public String ParameterTypesString;
    public String parameter;

    public int estimateSize() {
        return 4 + Method.length() + ParameterTypesString.length() + parameter.length() + 20;
    }
    private static AtomicInteger Id= new AtomicInteger();
    public ActRequest(){
        //apid = Id.getAndAdd(1);
    }

}