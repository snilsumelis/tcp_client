# tcp_client
# HTTP ve Soket İletişimi Örneği

Bu örnek uygulama, Texas Instruments Tiva C Series Tıva4c mikrodenetleyici kartı üzerinde çalışan bir HTTP GET isteği ve soket iletişimi örneğidir. Bu örnekte, HTTP GET isteği kullanılarak hava durumu verileri alınmakta ve alınan veriler soket ile başka bir cihaza gönderilmektedir.

## Kod Açıklaması

Bu kod bloğu aşağıdaki işlevleri gerçekleştirir:

- HTTP GET isteği göndererek, belirli bir hava durumu servisinden veri alır.
- Alınan hava durumu verilerini bir soket aracılığıyla belirli bir IP adresine ve port numarasına gönderir.
- Bir soket üzerinden gelen verileri işleyerek, belirli komutlara yanıt verir.
- İki farklı görev (task) oluşturur: `httpTask` ve `clientSocketTask`.
- İki farklı soket işlemini gerçekleştiren bir `serverSocketTask` görevi oluşturur.
- HTTP GET isteği, soket iletişimi ve veri işleme işlemleri birbiriyle senkronize bir şekilde çalışır.

## Kullanılan Donanım ve Kütüphaneler

- Texas Instruments Tiva C Series TM4C1294XL mikrodenetleyici kartı
- TI-RTOS (TI Gerçek Zamanlı İşletim Sistemi)
- TI-DRIVERS Kütüphanesi
- TI-SYSBIOS Kütüphanesi
- TI-NET Kütüphanesi

