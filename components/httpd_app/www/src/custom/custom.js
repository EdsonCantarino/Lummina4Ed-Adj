(function ($) {
    $.fn.extend({
        bs_alert: function (message, title) {
            var cls = 'alert-danger';
            var html = '<div class="alert ' + cls + ' alert-dismissable"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button>';
            if (typeof title !== 'undefined' && title !== '') {
                html += '<h4>' + title + '</h4>';
            }
            html += '<span>' + message + '</span></div>';
            $(this).html(html);
        },
        bs_warning: function (message, title) {
            var cls = 'alert-warning';
            var html = '<div class="alert ' + cls + ' alert-dismissable"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button>';
            if (typeof title !== 'undefined' && title !== '') {
                html += '<h4>' + title + '</h4>';
            }
            html += '<span>' + message + '</span></div>';
            $(this).html(html);
        },
        bs_danger: function (message, title) {
            var cls = 'alert-danger';
            var html = '<div class="alert ' + cls + ' alert-dismissable"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button>';
            if (typeof title !== 'undefined' && title !== '') {
                html += '<h4>' + title + '</h4>';
            }
            html += '<span>' + message + '</span></div>';
            $(this).html(html);
        },
        bs_info: function (message, title) {
            var cls = 'alert-info';
            var html = '<div class="alert ' + cls + ' alert-dismissable"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button>';
            if (typeof title !== 'undefined' && title !== '') {
                html += '<h4>' + title + '</h4>';
            }
            html += '<span>' + message + '</span></div>';
            $(this).html(html);
        },
        bs_success: function (message, title) {
            var cls = 'alert-success';
            var html = '<div class="alert ' + cls + ' alert-dismissable"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button>';
            if (typeof title !== 'undefined' && title !== '') {
                html += '<h4>' + title + '</h4>';
            }
            html += '<span>' + message + '</span></div>';
            $(this).html(html);
        }
    });
})(jQuery);

; (function ($) {
    $.fn.datetimepicker.dates['pt-BR'] = {
        format: 'dd/mm/yyyy',
        days: ["Domingo", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo"],
        daysShort: ["Do", "Se", "Te", "Qu", "Qu", "Se", "Sá", "Do"],
        daysMin: ["Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"],
        months: ["Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"],
        monthsShort: ["Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"],
        today: "Hoje",
        suffix: [],
        meridiem: []
    };
}(jQuery));

; $(function () {
    $('li.restart').on('click', function (e) {
        e.preventDefault();

        $('#msg_container').bs_danger('O dispositivo esta sendo reiniciado! <br />Aguarde que a pagina será recarregada!', 'Lummina 4 - Atenção');

        $.get("/restart");

        setTimeout(function () {
            location.reload();
        }, 20000);
    });
});