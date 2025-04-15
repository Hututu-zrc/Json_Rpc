#include <iostream>
#include <string>
#include<sstream>
#include <memory>
#include <jsoncpp/json/json.h>

// 实现数据的序列化
void Serialize(std::string * outstr)
{
    const char *name = "John";
    int age = 10;
    const char *sex = "man";
    float score[3] = {88, 85.2, 95};

    // 加入中间类当中去
    Json::Value student;
    student["name"] = name;
    student["age"] = age;
    student["sex"] = sex;
    student["score"].append(score[0]);
    student["score"].append(score[1]);
    student["score"].append(score[2]);

    Json::Value hobby;
    hobby["book"]="fairy tale";
    hobby["sports"]="badminton";
    student["hobby"]=hobby;
    Json::StreamWriterBuilder wb;
    std::unique_ptr<Json::StreamWriter> swptr(wb.newStreamWriter());
    std::stringstream ss;
    swptr->write(student,&ss);
    *outstr=ss.str();
}

// 实现数据的反序列化
void Deserialize(std::string & instr)
{
    //反序列化
    Json::Value student;

    Json::CharReaderBuilder cb;
    std::unique_ptr<Json::CharReader> crptr(cb.newCharReader());
    bool ret=crptr->parse(instr.c_str(),instr.c_str()+instr.size(),&student,nullptr);
    if(!ret)
    {
        std::cerr<<"CharReaderBuilder error"<<std::endl;
        exit(1);
    }
    std::string name=student["name"].asString();
    int age=student["age"].asInt();
    std::string sex=student["sex"].asString();
    float score[3];
    for(int i=0;i<3;i++)
        score[i]=student["score"][i].asFloat();
    Json::Value hobby=student["hobby"];
    std::string sports=hobby["sports"].asString();
    std::string book=hobby["book"].asString();
    std::cout<<"name: "<<name<<"\n";
    std::cout<<"age: "<<age<<"\n";
    std::cout<<"sex: "<<sex<<"\n";
    std::cout<<"score: "<<score[0]<<" "<<score[1]<<" "<<score[2]<<"\n";
    std::cout<<"sports: "<<sports<<"\n";
    std::cout<<"book: "<<book<<"\n";
  
}


int main()
{
    std::string str;
    Serialize(&str);
    Deserialize(str);
    return 0;
}
