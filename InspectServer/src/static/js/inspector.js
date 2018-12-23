/**

 */

$(function() {


    // DOM manipulation
    // $(function () {
        $('#btnFilter').click(function (ev) {
            $.ajax({
                url:'/api/innerbox',
                method: 'get',
                data:{
                    ip: $('#ip').textbox('getValue')
                },
                success:function (data,status,xhr) {
                    result = data.result;
                    // alert(JSON.stringify(data));
                    $('#innerbox_list').datagrid({data:result});
                },
                error:function (xhr,status,error) {
                    
                }
            });

        });
        $('#btnQueryTimeout').click(function (e) {

        });

    // });

});