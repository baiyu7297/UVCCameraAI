package com.huawei.hiaidemo.bean;

import android.graphics.Bitmap;

public class ClassifyItemModel {
    private String top1Result;
    private String otherResults;
    private String classifyTime;

    public ClassifyItemModel(String top1Result, String otherResults, String classifyTime) {
        this.top1Result = top1Result;
        this.otherResults = otherResults;
        this.classifyTime = classifyTime;
    }

    public String getTop1Result() {
        return top1Result;
    }

    public String getOtherResults() {
        return otherResults;
    }

    public String getClassifyTime() {
        return classifyTime;
    }

    @Override
    public String toString() {
        return "ClassifyItemModel{" +
                "top1Result='" + top1Result + '\'' +
                ", otherResults='" + otherResults + '\'' +
                ", classifyTime='" + classifyTime + '\'' +
                '}';
    }
}


