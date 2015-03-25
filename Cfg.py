#!/usr/bin/python3

"""
This program is part of MoaT.
Its job is simply to extract values from a YAML-formatted config file.
"""

import yaml

#_depth = -2
#def arm(p):
#    def wrap(*a,**k):
#        global _depth
#        _depth += 2
#        s=" "*_depth
#        print(s+">",repr((p.__name__,a,k)))
#        try:
#            #if a[1] == ["test","0"]:
#            #    import pdb;pdb.set_trace()
#            r = p(*a,**k)
#        except Exception:
#            print_exc()
#            raise
#        else:
#            print(s+"<"+repr(r))
#            #if r is None:
#            #    import pdb;pdb.set_trace()
#            return r
#        finally:
#            _depth += -2
#    return wrap

#@arm
def get1(ak,k):
    try:
        try:
            return ak[k]
        except KeyError:
            raise KeyError((ak,k))
    except Exception as e:
        if isinstance(k,str):
            #if k == "test.1":
            #    import pdb;pdb.set_trace()
            try:
                k = int(k)
            except ValueError:
                raise e
            else:
                try:
                    return ak[k]
                except KeyError:
                    raise KeyError((ak,k))

#@arm
def getpath(s,ak,k1,*rest):
    try:
        try:
            v1 = get1(ak,k1)
            if rest:
                v1 = getpath(s,v1,*rest)
            return v1
        except (KeyError,AttributeError) as e:
            try: v1 = get1(ak,'_default')
            except KeyError: raise e
            if rest:
                v1 = getpath(s,v1,*rest)
            return v1
    except (KeyError,AttributeError) as e:
        try: v = ak['_ref']
        except KeyError: raise e
        return getpath(s,s,*(v.split('.')+[k1]+list(rest)))

def _getkeys(lk,s,ak,*rest):
    if rest:
        if isinstance(ak,list):
            _getkeys(lk,s,ak[int(rest[0])],*rest[1:])
        else:
            if rest[0] in ak:
                _getkeys(lk,s,ak[rest[0]],*rest[1:])
            if '_default' in ak:
                _getkeys(lk,s,ak['_default'], *rest[1:])
    else:
        for k in ak:
            lk.add(k)
    if isinstance(ak,dict) and '_ref' in ak:
        _getkeys(lk,s,s,*(ak['_ref'].split('.')+list(rest)))

class Cfg(object):
    """\
        Encapsultates looking up items in a config tree.
        """
    def __init__(self, f):
        with open(f) as fd:
            self.data = yaml.load(fd)

    def subtree(self,*key):
        return getpath(self.data,self.data, *key)

    def keyval(self,*key):
        lk = set()
        _getkeys(lk,self.data,self.data,*key)
        for k in lk:
            if k.startswith('_'):
                continue
            kp=key+(k,)
            v = self.subtree(*kp)
            if isinstance(v,(int,str,float)):
                yield k,v

