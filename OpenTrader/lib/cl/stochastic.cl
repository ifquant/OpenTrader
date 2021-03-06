/*****************************************************************************
 * Copyright (c) 2017 OpenTrader
 * All rights reserved.
 *
 *  Author : Ali Khalilvandi
 *****************************************************************************/

 #define BUY		1
 #define SELL		2

 #define _DBG


__kernel void stochastic_kernel(__global int* ind_arr,__global int* prices,
					    __global int* params, __global int* result, 
					    size_t count,size_t ma_count)
{
    const int id     = get_global_id(0);
	int p_slowing = (params[id]) & 0xFF;
	int p_K = (params[id] >>  16) & 0xFF;
	int p_D = (params[id] >>  24) & 0xFF;
    int log_more=0;
	if(p_K==4 && p_D==4)
	    log_more=1;

#ifdef _DBG
 //   if(p_slowing==4)
//    printf("macd [%d,%d,%d]\n",p_ma1,p_ma2,p_slowing);
#endif

	int i;
	int spread=2;
	int curr_stoch_main,curr_stoch_sig ;
	int pre_stoch_main=0,pre_stoch_sig=0 ;
	int pos_type=0,open_price;
	int total_profit=0,trades_count=0,profit;

	long sum_main=0;

	for(i=0;i<count;++i){ 
	   curr_stoch_main = ind_arr [ ((p_K-1)*count) + (i)];
	   if(i < p_D){ 
	 	    sum_main += curr_stoch_main;
	     	curr_stoch_sig = (sum_main/(i+1));
			continue;
	   }
	   curr_stoch_sig -= (curr_stoch_sig/p_D);
	   curr_stoch_sig += (curr_stoch_main/p_D);
//	   if(curr_stoch_main>0)
	//      printf("stochastic %d,%d\n",p_K,curr_stoch_main);
		  if(log_more )
			printf("main=%d,sig=%d\n",curr_stoch_main,curr_stoch_sig);
		if (curr_stoch_main>curr_stoch_sig && pre_stoch_main<=pre_stoch_sig)
	   { 
	      if(pos_type == SELL)
		  { 
		     trades_count++;
			 profit = open_price - prices[i];
		     total_profit += profit;
			 total_profit -= spread; 
			 if(log_more)
			    printf("close sell @%d, total profit=%d,%d\n",prices[i],profit,total_profit);
		  }
	      pos_type = BUY;
		  open_price = prices[i];
		  if(log_more)
			printf("buy  @%d\n",open_price);
	   }

		if (curr_stoch_main<curr_stoch_sig && pre_stoch_main>=pre_stoch_sig)
	   { 
	      if(pos_type == BUY)
		  { 
		     trades_count++;
			 profit =  prices[i] - open_price;
		     total_profit += profit; 
			 total_profit -= spread; 
			 if(log_more)
			    printf("close buy @%d, total profit=%d,%d\n",prices[i],profit,total_profit);
		  }
	      pos_type = SELL;
		  open_price = prices[i];
		  if(log_more)
			printf("sell @%d\n",open_price);
	   }
		pre_stoch_main=curr_stoch_main;  
		pre_stoch_sig=curr_stoch_sig;  
    }
#ifdef _DBG
   // printf("profit [%d,%d]=%d\n",p_K,p_D,total_profit);
#endif
    result[id] = total_profit;
}
