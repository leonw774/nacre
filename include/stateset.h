unsigned char* new_stateset(int size);
void stateset_add(unsigned char* self, int pos);
void stateset_remove(unsigned char* self, int pos);
int stateset_find(unsigned char* self, int pos);
void stateset_union(unsigned char* self, unsigned char* right);
void stateset_intersection(unsigned char* self, unsigned char* right);
