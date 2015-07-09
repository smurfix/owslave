#!/usr/bin/env python3

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
class Cfg(object):
    """\
        Encapsultates looking up items in a config tree.
        """
    follow = True
    iseq = 1

    def __init__(self, f):
        with open(f) as fd:
            self.data = yaml.load(fd)
        self.ipath = {}
        self.data['_idata_'] = {}

    def inc1(self,f):
        i = self.ipath.get(f,None)
        if i is None:
            i = str(self.iseq)
            self.iseq += 1
            with open(f) as fd:
                self.data['_idata_'][i] = yaml.load(fd)
            self.ipath[f] = i

        return "_idata_."+i

    def inc(self,ak):
        "parse _ref entries"
        r = ak.get('_ref',None)
        if r is None:
            ak['_ref'] = r = []
        elif not isinstance(r,list):
            ak['_ref'] = r = [r]

        ri = ak.get('_include',None)
        if ri is not None:
            if isinstance(ri,list):
                for rin in ri:
                    r.append(self.inc1(rin))
            else:
                r.append(self.inc1(ri))

            del ak['_include']

        if not r:
            del ak['_ref']
        return iter(r)

    def getpath(self,ak,k1,*rest):
        try:
            try:
                v1 = get1(ak,k1)
                if rest:
                    v1 = self.getpath(v1,*rest)
                return v1
            except (KeyError,AttributeError) as e:
                if not self.follow: raise
                try: v1 = get1(ak,'_default')
                except KeyError: raise e
                if rest:
                    v1 = self.getpath(v1,*rest)
                return v1
        except (KeyError,AttributeError) as e:
            if not self.follow: raise
            for v in self.inc(ak):
                try:
                    return self.getpath(self.data,*(v.split('.')+[k1]+list(rest)))
                except KeyError:
                    pass
            raise e

    def getkeys(self,lk,ak,*rest):
        if rest:
            if isinstance(ak,list):
                self.getkeys(lk,ak[int(rest[0])],*rest[1:])
            else:
                if rest[0] in ak:
                    self.getkeys(lk,ak[rest[0]],*rest[1:])
                if self.follow and '_default' in ak:
                    self.getkeys(lk,ak['_default'], *rest[1:])
        else:
            for k in ak:
                lk.add(k)
        if self.follow and isinstance(ak,dict):
            if not rest or rest[0] != '_idata_':
                for v in self.inc(ak):
                    self.getkeys(lk,self.data,*(v.split('.')+list(rest)))


    def subtree(self,*key):
        return self.getpath(self.data, *key)

    def keyval(self,*key):
        lk = set()
        self.getkeys(lk,self.data,*key)
        for k in lk:
            if k.startswith('_'):
                continue
            kp=key+(k,)
            v = self.subtree(*kp)
            if isinstance(v,(int,str,float)):
                yield k,v

