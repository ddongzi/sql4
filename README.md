https://www.sqlite.org/arch.html
重构了sqlite，
该分支去实现： sql编译器。


orange:
实现的语句：
select a,b from tb;
select a,b from tb;selct name,age from tb;

create table tb (name,age);
select type,name,sql,root_pagenum from master;

insert into users (name,sex) values (wang,women);
select name,sex from users;

需要思考：
table 在哪？
执行字节存储时候，都是 <len1><data1><len2><data2> 的内容。不感知类型
table，需要一个翻译器， 生成/读取这些字节。或许甚至还涉及字段之间排序 填充等工作。
这也涉及到orange 类型解析
