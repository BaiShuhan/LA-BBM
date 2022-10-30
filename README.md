# LA-BBM (Layer-Aware fine-grained Bad Block Management algorithm) for 3D CT NAND Flash

<p align = "justify">
NAND flash blocks that are no longer reliable for storing and retrieving data are referred to as bad blocks. Flash devices can ship from the factory with bad blocks that originated in the manufacturing process. Bad blocks also arise as repeated program/erase operations gradually wear out the tunnel oxide of flash cells. Therefore, flash-based storage systems employ a bad block management (BBM) algorithm to mark bad blocks so that they will no longer be used. Traditional BBM algorithms mark a flash block as bad once any of its pages fail. Due to the huge reliability variations across pages within a block, it is not space-efficient to use the traditional BBM algorithm for 3D NAND flash.
</p>

<p align = "justify">
We comprehensively characterize the performance and reliability features of the 3D CT NAND flash and summarize critical observations. Two observations are attractive. First, page lifetime within a block has a *significant difference*, as shown in the following figure:
</p>

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/%20Page%20lifetime%20distribution%20within%20a%20block.png)

<p align = "justify">
Second, the lifetime distribution of different WLs in a flash block exhibits *intra-layer concentration* and *cross-layer variation*, as shown in the following figure:
</p>

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/%20Lifetime%20distributions%20of%20WLs%20within%20a%20block.png)

<p align = "justify">
Based on these observations,  we propose a layer-aware fine-grained bad block management algorithm, LA-BBM, to improve the utilization of storage space and, thus, the lifetime of flash storage.
</p>

<p align = "justify">
If the page RBER exceeds the maximum error correction capability, the page is regarded as the failed page. LA-BBM only marks pages in the layer containing the failed page as bad and then only migrates valid data in that layer. The following figure compares the traditional BBM algorithm and LA-BBM.
</p>

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/%20Traditional%20BBM%20algorithm%20and%20LA-BBM.png)

<p align = "justify">
To simplify the expression, only Block 0 and Block 1 are shown, and only 6 layers (not the actual 48 layers) are drawn in each block for demonstration purpose. There are 4 WLs in each layer, represented by small squares, and the different colors of the squares represent the different states of the WLs (free, valid, invalid, and failed). When a page in WL21 fails, instead of viewing the whole Block 0 as a bad block and migrating all of the valid data therein, LA-BBM only marks WL21, WL22, WL23, and WL24 to the failed state and then migrates the valid data in WL21, WL22, and WL23.
</p>

<p align = "justify">
Marking a flash block as bad after all its layers fail can maximize the utilization of storage space and, thus, the storage lifetime. However, as the available space in each flash block decreases due to layer failures, garbage collection operations are triggered more frequently, degrading the performance. There exists a tradeoff between the lifetime and performance in deciding when to mark the entire flash block as bad as the layers in it fail gradually. We set a threshold named layer failure threshold. When the ratio between the number of failed layers and the number of all the layers in a flash block exceeds the layer failure threshold, the entire flash block is marked as bad and will not be used anymore. 
</p>

<p align = "justify">
We maintain a bitmap to record the failed or healthy state of each layer. As for BiCS2 TLC from Toshiba, a representative 3D CT NAND flash product we choose to conduct experiments, its chip has a capacity of 2.48Tb (310GB) and contains 31552 blocks. Considering every block has 48 layers, the bitmap size is about 185 (=(31552×48×1bit)/(8×1024)) KB, indicating a negligible space overhead.
</p>

<p align = "justify">
Simulation results show that LA-BBM can significantly improve the SSD lifetime (30.6%-62.2%) with a small performance degradation (less than 10% increase of tail I/O response time), compared to the traditional bad block management algorithm.
</p>

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/The%20increase%20of%20the%20maximum%20volume%20of%20flash%20writes.png)

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/%20The%20increase%20of%20the%20maximum%20volume%20of%20user%20data%20writes.png)

![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/IO%20response%20time_exchange.png)
![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/IO%20response%20time_hm0.png)
![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/IO%20response%20time_usr0.png)
![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/IO%20response%20time_ts0.png)
![image](https://github.com/BaiShuhan/LA-BBM/blob/main/figures/IO%20response%20time_vps.png)
