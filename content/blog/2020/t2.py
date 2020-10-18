#!/usr/bin/env python3

import glob
import frontmatter
import datetime
import pytz
utc=pytz.UTC


author_list=[]
date_list=[]
near_date_list=[]
day_time = 7    #7天内文章，可修改



now = datetime.datetime.now()
last_time = now - datetime.timedelta(day_time) 
now = now.replace(tzinfo=utc)


def main():
    files = glob.glob('*/index.md')
    print("\n\n\n最近%d天内提交文章详情:"%day_time,"%d-%d-%d"%(last_time.year,last_time.month,last_time.day),'>>',"%d-%d-%d"%(now.year,now.month,now.day))
    print("========================================")
    #print(last_time.date,"--",now.date)
    for f in files:
        get_author(f)
        get_date(f)
    #count_num(author_list)
   # print("====================")
    print("\n\n累计提交文章详情：")
    print("====================")
    format_print(count_num(author_list))

def get_author(md_file):    
    md = frontmatter.load(md_file)
    author_list.append(md.get('author'))

def compare_time(file_time,md):
    if (((now - date_list[-1]).days)<=day_time):
         print("%d-%d-%d"%(file_time.year,file_time.month,file_time.day),"-",md.get('title'),"-",md.get('author'))
         print()
def get_date(md_file):
    md = frontmatter.load(md_file)
    date_list.append(md.get('date'))
    file_time = md.get('date')
    compare_time(file_time,md)
    
   # print(now)
   # print(date_list[-1])




def count_num(arr):
    result = {}
    for i in set(arr):
        result[i] = arr.count(i)
    return result

def format_print(print_list):
    print_list=sorted(print_list.items(),key=lambda d:d[1],reverse=True) #按值来排序 如果是False 则为正序
    # print(author_list)
    print(" 作者     文章数")
    print("=================")
    for index, val in enumerate(print_list):
        print(val[1],"\t-", val[0])


if __name__ == '__main__':
    main()





