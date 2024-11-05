# TESO Watcher

## Преамбула
В марте 2024 [Zenimax Online Studio](https://www.zenimax.com/) (ZOS) выпустили Update 41 к своей игре [The Elder Scrolls Online](https://www.elderscrollsonline.com/). 
В нем они ввели какую-то античитерскую систему (или улучшили существующую), после чего игра стала "вылетать" при запуске, если в памяти находится [Sysinternals Process Explorer](https://learn.microsoft.com/en-us/sysinternals/downloads/process-explorer).
Причём, "вылетает" игра не сразу - ты вводишь пароль, успеваешь войти в "лобби" и даже почти успеваешь загрузиться в стартовую локацию после выбора персонажа.. и тут бах! - диалог с ошибкой, начинай сначала.

Естественно, игроки создали тему на [форуме](https://forums.elderscrollsonline.com/en/discussion/654126/game-crashes-when-process-explorer-from-microsofts-sysinternals-is-running) и даже какие-то тикеты в поддержку. 
ZOSы, конечно, обещали разобраться, но сейчас у нас ноябрь 2024, за плечами уже Update 44, а воз (ZOS 😉) и ныне там. ZOSы нихрена не сделали для исправления этого бага и, судя по всему, и не собираются.

Конечно, подавляющему большинству пользователей ProcessExplorer он нафиг не нужен в процессе игры. Но фишка в том, что он, как правило, запускается при старте системы, да и руками тоже в течение дня запускают и не закрывают, а сворачивают в трей, и он, по сути, все время висит себе в фоне.
Поэтому процесс запуска иргы выглядит так - запустили TESO, подождали, ввели пароль (ну у кого стим версия тем чуть проще), подождали "лобби", выбрали перса, подождали загрузки локации, "вылетели", послали ZOSов "по-адресу", закрыли ProcessExlorer, запустили TESO, пароль, загрузка, лобби, загрузка, все наконец-то игра.

Это бесит. Забыть закрыть ProcessExplorer перед запуском TESO - умеем, могем, как нефиг делать...

## Амбула
Короче мне это надоело. Я написал эту програмку, котрая висит в памяти и ждет, когда запустится TESO. Когда она это "ловит", она ищет все процессы ProcessExplorer'a и закрывает их. Всё.

Чтобы проворачивать такое и не просить каждый раз права администратора, программа написана как Windows Service и должна запускаться под учетной записью LocalSystem.

## Установка
Качаете инсталятор из [релизов](https://github.com/alezhu/teso_vs_procexp/releases), запускаете. Всё.

## Деинсталяция
Идете в панель управления в управление программами, ищите там "TESO Wait Service" и удаляете. Всё.