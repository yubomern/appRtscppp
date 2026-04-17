import hashlib
import json 
import time

class Block :

    def __init__(self,index,timestamp , data , previous_hash):
        self.index = index 
        self.timestamp = timestamp
        self.data  = data  
        self.previous_hash = previous_hash
        self.nonce = 0 
        self.hash  = None  
    def calculate_hash  (self) :
        block_string  = json.dumps(self.__dict__ , sort_keys=True)
        return hashlib.sha256(block_string.encode()).hexdigest()
    

class BlockChain :
    def __init__(self):
        self.chain   = [self.create_genesis_block()]

    def create_genesis_block(self) :
        return Block(0,time.time() ,  "Genisis block" ,"0")
    
    def get_last_block(self):
        return self.chain[-1]
    
    def  add_cblock(self,new_block):
        new_block.previous_hash = self.get_last_block().hash 
        new_block.hash = new_block.calculate_hash()
        self.chain.append(new_block)
    

if __name__ =='__main__' :
    block =BlockChain()
    block.add_cblock(Block(1,time.time() , "trans" , "1"))
    print(block.get_last_block().hash)

