#ifndef _FIFO_H_
#define _FIFO_H_

#include <string.h>
template<typename T,int S>
class fifo
{

private:
	T	data[S];
	int	ini;
	int	end;
	int	len;
public:

	fifo()
	{
		ini=0;
		end=0;
		len=0;
	}
	
	int push(const T *in,int l)
	{
		//Comprobamos que quepa
		if (size()<len+l)
			return 0;
	
		//Calculamos lo que queda libre hasta el final
		int free = size()-end;
	
		//Y si tenemos que copiar al principio
		if (free>=l)
		{
			//Cabe todo 
			memcpy(data+end,in,l*sizeof(T));
	
			//Movemos el endal
			end+=l;

			//Check end
			if (end==size())
				//From the begining
				end=0;
		} else {
			//No cabe todo
			memcpy(data+end,in,free*sizeof(T));
			memcpy(data,in+free,(l-free)*sizeof(T));
	
			//Movemos el endal
			end=l-free;
		}
	
		//Incrementamos el tama�o
		len+=l;
	
		return l;
	}

	T pop()
	{
		T out;
		pop(&out,1,true);
		return out;
	}

	T peek()
	{
		T out;
		pop(&out,1,false);
		return out;
	}

	int pop(T *out,int l)
	{
		return pop(out,l,true);
	}

	int peek(T *out,int l)
	{
		return pop(out,l,false);
	}
	
	int pop(T *out,int l,bool remove)
	{
		//Comprobamos que quepa
		if (len<l)
			return 0;
	
		//Calculamos lo que queda hasta el end
		int total = size()-ini;
	
		//Y si tenemos que copiar al principio
		if (total>=l)
		{
			//Cabe todo 
			memcpy(out,data+ini,l*sizeof(T));
	
			//Movemos el principio
			if (remove)
				ini+=l;
			//cehck if we hav past the end
			if (ini==size())
				ini=0;
		} else {
			//El resto esta al principio
			memcpy(out,data+ini,total*sizeof(T));
			memcpy(out+total,data,(l-total)*sizeof(T));
	
			//Movemos el principio
			if (remove)
				ini=l-total;
		}
	
		//Decrementamos el tama�o
		if (remove)
			len-=l;
	
		return l;
	}

	int remove(int l)
	{
		//Comprobamos que quepa
		if (len<l)
			return 0;
	
		//Calculamos lo que queda hasta el end
		int total = size()-ini;
	
		//Y si tenemos que copiar al principio
		if (total>=l)
		{
			//Movemos el principio
			ini+=l;
		} else {
			//Movemos el principio
			ini=l-total;
		}
	
		//Decrementamos el tama�o
		len-=l;
	
		return l;
	}
	int size()
	{
		return S;
	}
	int length()
	{
		return len;
	}

	void clear()
	{
		//MOvemos la posicion hasta el endal
		ini = end;

		//Y ponemos la longitud a cero
		len = 0;
	}
	
};

#endif
