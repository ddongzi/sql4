https://www.sqlite.org/arch.html
重构了sqlite，
该分支去实现： sql编译器。


orange:
实现的语句：
select a,b from tb;
select a,b from tb;selct name,age from tb;

create table tb (name,age);

select type,name,sql,root_pagenum from master;