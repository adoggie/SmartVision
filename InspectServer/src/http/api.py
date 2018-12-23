#coding:utf-8

import json
from flask import Flask,request,g
from flask import Response
import requests
import base64
from StringIO import StringIO

from flask import render_template
from mantis.fundamental.application.app import  instance
from mantis.fundamental.utils.useful import cleaned_json_data

from mantis.fundamental.flask.webapi import ErrorReturn,CR




def get_innerbox():
    """"""
    main = instance.serviceManager.get('main')
    ip = request.values.get('ip')

    # mongo = instance.datasourceManager.get("mongodb").conn
    model = main.getDbModel()

    if ip:
        rs = model.InnberBoxCheckSnapShot.collection().find({'ip':{'$regex':ip}}).sort('ip',1)
    else:
        rs = model.InnberBoxCheckSnapShot.collection().find().sort('ip',1)

    result =  []
    for r in list(rs):
        del r['_id']
        result.append(r)
    return CR(result= result).response
