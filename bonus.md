Please describe any bonuses you implemented (this file is included in your submission)
1. Bonus Tail
2. Bonus Heap
3. Bonus Garbage(conservative garbage collector)
- First, I have declared a constant called "INDICATOR" with the value 
"22400000" as part of the stack struct. Then, whenever a newArray function is 
called, add the INDICATOR + reference(which corresponds to an 
index in the array matrix "arrays" where the array is stored) to an array 
called gc. Next, in gc_func, check if the arrays in gc is used by checking the 
INDICATOR + reference exists. If unused, free. And lastly, if there are any 
circular reference, remove.