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

init_product_list()


