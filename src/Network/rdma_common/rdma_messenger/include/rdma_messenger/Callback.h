#ifndef CALLBACK_H
#define CALLBACK_H

class Callback {
  public:
    virtual ~Callback() {}
    virtual void entry(void *param, void *msg = nullptr) = 0;
};

#endif
