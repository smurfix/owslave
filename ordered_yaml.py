#
import yaml
from collections import OrderedDict

def load(stream, Loader=yaml.Loader, object_pairs_hook=OrderedDict):
    class OrderedLoader(Loader):
        pass
    def construct_mapping(loader, node):
        loader.flatten_mapping(node)
        return object_pairs_hook(loader.construct_pairs(node))
    OrderedLoader.add_constructor(
        yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,
        construct_mapping)
    return yaml.load(stream, OrderedLoader)

def dump(data, stream=None, Dumper=yaml.Dumper, **kwds):
    class OrderedDumper(Dumper):
        pass
    def _dict_representer(dumper, data):
        return dumper.represent_mapping(
            yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,
            data.items(), flow_style=False)

    def literal_presenter(dumper, data):
        return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='')

    def list_presenter(self, data):
        return self.represent_sequence(u'tag:yaml.org,2002:seq', data, flow_style=False)
    def dict_presenter(self, key, data):
        return self.represent_sequence(u'tag:yaml.org,2002:seq', key, data, flow_style=False)

    OrderedDumper.add_representer(str, literal_presenter)
    OrderedDumper.add_representer(tuple, list_presenter)
    OrderedDumper.add_representer(list, list_presenter)

    kwds.update(dict(allow_unicode=True, encoding='utf-8'))

    OrderedDumper.add_representer(OrderedDict, _dict_representer)
    return yaml.dump(data, stream, OrderedDumper, **kwds)

