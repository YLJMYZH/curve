import hashlib
import random
import re
import time
import requests
import os

# 百度api
apiurl = 'http://api.fanyi.baidu.com/api/trans/vip/translate'
appid = '20230714001744725' # 百度翻译api的appid
secretKey = 'y9WUHMVjoliF29rDWCNV' # 百度翻译api的secretKey

# 读取文件，翻译成英语，返回一个翻译好的文档的每行的list
def text_deal(filename, fromLang='zh', toLang='en'):
    file = open(filename, encoding="utf-8")
    while 1:
        lines = file.readlines(10000)
        if not lines:
            break
        # 遍历每一行代码
        i = 0
        newline = []
        for line in lines:
            i += 1
            lineat = line.lstrip()  # 去除了左方空行
            # 判断是否是注释，不是注释则跳过
            linea = lineat[lineat.find('//'):]
            # 查找中文字符
            chineses = find_chinese(linea)
            if chineses:
                print('-----------------------------------------------------')
                print("[" + str(i) + "]: ", linea)
                # 查找到中文字符，并调用翻译api翻译进行字符串替换
                for c in chineses:
                    time.sleep(1)
                    translate_str = translateBaidu(c, fromLang, toLang)
                    line = line.replace(c, translate_str)
                    print(c + " ----> " + translate_str)
                print('-----------------------------------------------------')
            newline.append(line)
    file.close()
    return newline

# 查找文本中的中文字符，并将连续的中文字符以列表返回
def find_chinese(text):
    regStr = ".*?([\u4E00-\u9FA5]+).*?"
    aa = re.findall(regStr, text)
    return aa

# 翻译内容源语言翻译后的语言
def translateBaidu(content, fromLang='zh', toLang='en'):
    salt = str(random.randint(32768, 65536))
    sign = appid + content + salt + secretKey  # appid+q+salt+密钥的MD5值
    sign = hashlib.md5(sign.encode("utf-8")).hexdigest()  # 对sign做md5，得到32位小写的sign
    try:
        # 根据技术手册中的接入方式进行设定
        paramas = {
            'appid': appid,
            'q': content,
            'from': fromLang,
            'to': toLang,
            'salt': salt,
            'sign': sign
        }
        response = requests.get(apiurl, paramas)
        jsonResponse = response.json()  # 获得返回的结果，结果为json格式
        dst = str(jsonResponse["trans_result"][0]["dst"])  # 取得翻译后的文本结果
        return dst
    except Exception as e:
        print(e)

# 调用函数处理文档
files = os.listdir("./translate")
for file in files:
    print("=====================")
    print("正在处理文件：" + file)
    newlines = text_deal("./translate/" + file, "zh", "en")
    fw = open("./translated/" + file, "a",encoding="utf-8")
    for l in newlines:
        fw.write(l)
    fw.close()