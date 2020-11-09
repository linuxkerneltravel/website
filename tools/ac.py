#!/usr/bin/env python3
import glob
import frontmatter
import datetime
import pytz
#from datetime import datetime, timezone

utc=pytz.UTC

author_list=[]
date_list=[]
near_date_list=[]
day_time = 7    #7天内文章，可修改

now = datetime.datetime.now()
last_time = now - datetime.timedelta(day_time) 
line = "<br>========================================"


def main():
    files = glob.glob('../content/blog/2020/*/index.md')
    ly = str(last_time.year)
    lm = str(last_time.month)
    ld = str(last_time.day)
    ny = str(now.year)
    nm = str(now.month)
    nd = str(now.day)
    lastday = ly+'-'+ lm+'-'+ ld
    today = ny+'-'+nm+'-'+ nd
    date = lastday + ">>>" +today
    with open("ac.html","w") as html:
        
        html.write("最近7天提交详情<br>")
        html.write(date)
        html.write(line)
    for f in files:
        get_author(f)
        get_date(f)
    
    with open("ac.html","a") as html:
        html.write("<br><br>累计提交文章详情:")
        html.write(line)
        html.write("<br> 文章数 &emsp; &emsp; 作者")
        html.write(line)
    format_print(count_num(author_list))

def get_author(md_file):    
    md = frontmatter.load(md_file)
    author_list.append(md.get('author'))

def compare_time(file_time,md):
    if (((now - date_list[-1]).days)<=day_time):
        write_date = str(file_time.year)+'-'+str(file_time.month)+'-'+str(file_time.day)
        title = str(md.get('title'))
        author = str(md.get('author'))
        txt = '<br>'+write_date+' -- '+title+' -- '+author+'<br>'
        with open("ac.html","a") as html:
            html.write(txt)
    
def get_date(md_file):
    md = frontmatter.load(md_file)
    date_list.append(md.get('date'))
    file_time = md.get('date')
    compare_time(file_time,md)



def count_num(arr):
    result = {}
    for i in set(arr):
        result[i] = arr.count(i)
    return result

def format_print(print_list):
    print_list=sorted(print_list.items(),key=lambda d:d[1],reverse=True) #按值来排序 如果是False 则为正序
    for index, val in enumerate(print_list):
        count = str(val[1])
        author = str(val[0])
        txt = '<br>'+count + "&emsp;&emsp;&emsp;&emsp;-" +author
        with open("ac.html","a") as html:
            html.write(txt)


if __name__ == '__main__':
    main()





