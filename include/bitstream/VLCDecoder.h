#ifndef VLCDECODER_H_
#define	VLCDECODER_H_
#include "config.h"
#include "tools.h"
#include <stdexcept>
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

template<typename T>
class VLCDecoder
{
public:
	inline void AddValue(DWORD code,BYTE len,T value)
	{
		BYTE aux[4];
		//Create writter
		BitWriter writter(aux,4);
		//Write data
		writter.Put(len,code);
		//Flush to memory
		writter.Flush();
		//Init the bit reader with the code
		BitReader reader(aux,len);
		//Start from the parent node
		Node *n=&table;
		//Iterate the tree
		for(BYTE i=0;i<len;i++)
		{
			//Get bit
			DWORD child = reader.Get(1);
			//chek not empty
			if(!n->childs[child])
				//Create it
				n->childs[child] = new Node();
			//Get next node
			n = n->childs[child];
		}
		//Set the value
		n->value = value;
	}

	inline T Decode(BitReader &reader)
	{
		//Debug("VLC [");
		//Start from the parent node
		Node *n=&table;
		//Iterate the tree
		while(!n->value)
		{
			BYTE v = reader.Get(1);
			//Debug("%d",v);
			//Get next node
			n = n->childs[v];
			//check valid node
			if (!n)
				//No value found, erroneus code
				return NULL;
		}
		//Debug("]\n");
		//Return found value
		return n->value;
	}
private:
	struct Node
	{
		Node()
		{
			childs[0] = NULL;
			childs[1] = NULL;
			value = NULL;
		}
		Node* childs[2];
		T value;
	};

	Node	table;
};

#endif	/* VLCDECODER_H_ */
