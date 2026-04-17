import hashlib
import json
import time 
import  sys
if  len(sys.argv) > 1 :
    a,b,c,d = sys.argv[1] if sys.argv[1] else "1",sys.argv[2] if sys.argv[2] else "hello" ,  sys.argv[3] if sys.argv[3] else "0" , int(sys.argv[4]) if sys.argv[4] else 1
else  :
    a,b,c,d  = "1", "hello" , "10", 1


def add(index , data ,previsoushash ,nonce = 0 ):
    app_dct = {
        "index" :  index ,
        "timestamp" :  time.time(),
        "data" :data,
        "previousHash" :previsoushash ,
        "nonce" : nonce

    }
    return app_dct
dct = add(a,b,c,d)
block_string  = json.dumps(dct, sort_keys=True)
secret =  hashlib.sha256(block_string.encode()).hexdigest()
print(secret)

print(f"secret is  {secret}  ")