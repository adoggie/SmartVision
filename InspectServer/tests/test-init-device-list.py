# coding: utf-8

from mantis.fundamental.nosql.mongo import Connection
from mantis.BlueEarth import model
from mantis.fundamental.parser.yamlparser import YamlConfigParser
from mantis.fundamental.utils.timeutils import timestamp_current
from mantis.fundamental.utils.useful import object_assign

def get_database():
    db = Connection('BlueEarth').db
    return db

model.get_database = get_database

def init_product_list():
    content = YamlConfigParser('./product-list.yaml').props.get('product_list')
    
    for profile in content:
        product = model.Product.get_or_new(sku=profile['sku'])
        # product = model.Product()
        object_assign(product,profile)
        product.update_time = timestamp_current()
        product.save()


def init_device_list(filename,device_type):
    f =  open(filename)
    for line in f.readlines():
        line = line.strip()
        if not line or line[0] == '#':
            continue
        device = model.Device.get(device_id=line)
        if not device:
            device = model.Device.create(device_id=line,device_type=device_type,name = line[-6:])
            device.save()
            print line,device_type

init_device_list('device-list-ev25.txt','ev25')
init_device_list('device-list-gt03.txt','gt03')
init_device_list('device-list-gt310.txt','gt310')




