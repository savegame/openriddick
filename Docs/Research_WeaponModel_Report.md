# Отчёт по исследованию: `m_iModel[0] == 0` у предметов в руках

Дата: 2026-07-30. Спека: `Docs/Research_WeaponModel.md`. Только чтение, код не менялся.

Краткий итог: ноль в `m_iModel[0]` рождается на сервере, в `case MODEL` парсера
предмета (либо ключ `MODEL` вообще не доезжает до парсера). Репликация и
рендер ни при чём — они честно возят ноль. Две рабочие гипотезы:

1. В вычисленном RPG-шаблоне оружия нет ключа `MODEL`, а модель объявлена
   через `ATTACHMODEL0` — который по коду пишет в **слот 1**, а не слот 0
   (см. §2.2). Тогда `m_iModel[0]` законно остаётся 0.
2. `CreateObject` предмета случается после выставления
   `WMAPDATA_STATE_NOCREATE` — тогда `GetResourceIndex` возвращает 0, но при
   этом в логе обязана быть строка `Non-existing resource requested`
   (`WMapData.cpp:396-400`). В текущем `run_.log` её нет → гипотеза слабая.

Решающая улика — один зонд на `WRPGItem.cpp:355` (печать `KeyValue` +
вернувшегося индекса), см. §4.

---

## 1. Доходит ли `case MHASH2('MODE','L')` до выполнения

### 1.1. Цепочка вызовов парсера предмета

`OnEvalKey` предмета вызывается из `CRPG_Object::CreateObject` двумя путями:

- `Source/P5/Projects/Main/GameClasses/WRPG/WRPGCore.cpp:218-225` (создание
  по имени шаблона):

```cpp
spCRegistry spReg = GetEvaledRegistry(_pName, _pWServer);   // :207
...
int nKeys = spReg->GetNumChildren();
for(int k = 0; k < nKeys; k++)
{
    const CRegistry* pReg = spReg->GetChild(k);
    spItem->OnEvalKey(pReg->GetThisNameHash(), pReg);
}
spItem->OnFinishEvalKeys();
```

- `WRPGCore.cpp:250-265` — вариант от готового `CRegistry*`; зовётся, напр.,
  из `WObj_CharCreate.cpp:2719` (`CreateObject(m_spRPGObjectReg, ...)`).

Спавн/выдача оружия по имени идёт через `CreateObject`:
`WObj_CharIO.cpp:1216` (загрузка инвентаря), `WObj_CharMsg.cpp:7387`,
`WObj_GameMod.cpp:6648` и др.

Дальше экипировка: `CWObject_Character::OnUpdateItemModel`
(`WObj_CharMechanics.cpp:5968`) копирует серверный `pItem->m_Model` в
автопеременную клиента через `UpdateModel`
(`WObj_AutoVar_AttachModel.cpp:136-143` — копирует все `m_iModel[i]`,
включая [0]).

### 1.2. Откуда берётся реестр предмета

Реестр — это **вычисленный RPG-шаблон**, а не файл предмета:

- `GetEvaledRegistry` (`WRPGCore.cpp:124`): сначала кэш
  `SERVER\RPGTEMPLATES_EVAL` (`:142`), иначе `SERVER\RPGTEMPLATES` (`:172`) +
  `spTemplates->EvalTemplate_r(&Reg)` (`:187`).
- Корень — game-реестр мира: `_pMapData->m_spWData->GetGameReg()->Find(
  "SERVER\\RPGTEMPLATES")` (`WRPGCore.cpp:630`).

Т.е. источник данных — реестр внутри WData мира (.xw). Отдельного
WData-ресурса класса RPGITEM не существует (grep по
`WRESOURCE_CLASS_RPGITEM` пуст).

### 1.3. Case MODEL существует и активен в PC-retail

Декомпайл `GameClasses_Win32_x86_dll_decomp.c:401331`
(`FUN_10262dd0` = retail `CRPG_Object_Item::OnEvalKey`; опознан по строке
"Effect index out of range" на `:400927`):

```c
if (param_1 == (int *)0xff1eef1) goto LAB_1026306b;
// 0xff1eef1 = djb2("model") - 5381 = MHASH2('MODE','L')
```

Тело на `:401289-401296` — вызов `GetResourceIndex_Model` (vtable+0x68 у
MapData) и запись int16 в `this+0x1f2` (= `m_iModel[0]`). Значит PC-контент
штатно пользуется ключом MODEL, и ветка в ретейле живая.

ВАЖНО при сверке хешей с декомпайлом: `MHASH` — это djb2 **минус 5381**
(`MRTC_StringHash.h:31-41`).

### 1.4. Ветка «MODEL отсутствует в данных» — реальна

Case сработает только если в вычисленном шаблоне есть дочерний ключ
буквально `MODEL`. Ключи объявления модели у предмета (`WRPGItem.cpp`):
`MODEL` (:352), `PHYSMODEL` (:359), `ATTACHMODEL<n>` (default-ветка :746),
`EFFECTLIST`/`EFFECT<n>` (:703-714). `ATTACHROTTRACK`/`ATTACHPOINT` — только
аттач-параметры слота 0, модель не задают.

## 2. Пути заполнения `m_iModel[0]`

### 2.1. Кто может писать слот 0

Ровно два пути:

- `case MODEL` → прямое присваивание `m_Model.m_iModel[0]`
  (`WRPGItem.cpp:352-356`, дубль в `WRPGItem2.cpp:238-241`).
- `CAutoVar_AttachModel::SetBaseModel`
  (`WObj_AutoVar_AttachModel.cpp:155-161`).

`OnFinishEvalKeys` (`WRPGItem.cpp:807-821`) модель не трогает — отложенного
присваивания после eval'а ключей нет.

### 2.2. `ATTACHMODEL<n>` → слот n+1, не слот 0

`WRPGItem.cpp:746-774` (default-ветка `OnEvalKey`): парсит
`ATTACHMODEL<i> "<attachpoint>#<model>"`, причём **`Index = <i> + 1`**
(`:749`):

```cpp
int Index = KeyName.Copy(11, 1024).Val_int() + 1;
if(Index >= 1 && Index < ATTACHMODEL_NUMMODELS)
{
    ...
    int iModel = m_pWServer->GetMapData()->GetResourceIndex_Model(Str);
    ...
    if (iModel > 0)
        m_Model.SetModel(Index, iModel, iAttachPoint);
}
```

`SetModel` (`WObj_AutoVar_AttachModel.cpp:163-168`) — просто
`m_iModel[_Index] = _iModel; m_iAttachPoint[_Index] = _iAttachPoint;`.

Вывод: `ATTACHMODEL0` пишет слот **1**. Если у шаблона оружия в PC-данных
есть только `ATTACHMODEL0` и нет `MODEL`, `m_iModel[0]` законно останется 0,
а рендер-гейт `GetModel0_RenderInfo` смотрит именно слот 0. Закомментированный
`case MHASH3('ATTA','CHMO','DEL')` (`WRPGItem.cpp:362-369`) — голый
`ATTACHMODEL` без номера; в ретейле он тоже выключен.

## 3. `CMapData::GetResourceIndex` — полный путь и условия нуля

### 3.1. Lookup → create-on-demand → 0 только при NOCREATE/падении Create

`Source/P5/Shared/MOS/Classes/GameWorld/WMapData.cpp:384-421`:

```cpp
392  int iRc = (m_spHash != NULL) ? m_spHash->GetIndex(TmpName) : 0;
395  if (iRc > 0) return iRc;
396  if (m_State & WMAPDATA_STATE_NOCREATE)
398      ConOutL(CStrF("...ERROR: (CMapData::GetResourceIndex) Non-existing resource requested: %s", TmpName));
400      return 0;
403  int iWRc = m_spWData->GetResourceIndex(TmpName, _RcClass, this);   // create-on-demand
416      Hash_Insert(iRc);
418      return iRc;
```

WData-уровень (`WDataCore.cpp:2231-2268`): инстанцирует класс ресурса через
реестр и зовёт `Create`; `false`/исключение → 0.

**Важно:** `CWRes_Model_XMD::Create` (`WDataRes_Models.cpp:122-134`) **не
проверяет существование файла** — просто `m_Name = _pName; return true;`
(базовый `CWResource::Create`, `WData.cpp:22-42`, тоже всегда true). Файл
читается позже, в `OnLoad/ReadModel`. «Файла нет на диске» — НЕ причина
нулевого индекса.

### 3.2. Порядок инициализации таблицы vs спавн предметов

- Сервер: `World_Init` создаёт **пустой** CMapData
  (`WServer_World.cpp:849-857`: `m_spMapData = MNew(CMapData); Create(...);
  SetWorld(...)`). Таблица растёт по требованию при спавне объектов (предметы
  парсят ключ MODEL внутри `World_DoOnSpawnWorld`, `WServer_World.cpp:242`).
- `NOCREATE` на сервере ставится **после** спавна объектов уровня:
  `WServer_World.cpp:306` (конец загрузки мира); снимается в
  `WDeltaGameState.cpp:627`.
- На клиенте `NOCREATE` ставится **сразу** (`WClient_Core.cpp:245-248`,
  `:878-887` — `SetNumResources(_nRc)` + `SetState(NOCREATE)`); записи
  приходят сетевыми апдейтами. Но клиенту имена не нужны (см. §5.3).
- Путь `CMapData::Read` (`WMapData.cpp:1049-1083`, entry `"RESOURCES"`) —
  демо/клиентская репликация, на сервере не вызывается.

Диагностический маркер для run.log: при нуле из-за `NOCREATE` в логе
обязана быть строка `Non-existing resource requested` (для всех классов,
кроме WOBJECTCLASS) либо `File exception creating resource`. В текущем
`run_.log` таких строк нет.

## 4. Логирование и точка для зонда

Логирования на пути нет:

- `WRPGItem.cpp:353-356` — case MODEL молчит при любом исходе.
- `GetResourceIndex_Model` (`WMapData.cpp:500-548`) молчит; закомментированный
  `//LogFile(...)` на `:504`.
- `GetResourceIndex` печатает ошибку только в состоянии `NOCREATE` (см. §3.1).

**Осмысленная точка для одной строки диагностики — `WRPGItem.cpp:355`**
(внутри case MODEL, печатать `KeyValue` и вернувшийся индекс). Одна строка
отвечает на два вопроса сразу:

- строки в логе нет вовсе → ключа `MODEL` в шаблоне нет → смотреть
  ATTACHMODEL-ветку (`:746`) и сам шаблон в RPGTEMPLATES;
- строка есть, индекс 0 → проблема в резолвере/таблице;
- строка есть, индекс > 0 → ноль возникает позже (экипировка/репликация),
  тогда зондировать `OnUpdateItemModel` (`WObj_CharMechanics.cpp:5968`).

Альтернатива с меньшей точностью — раскомментировать LogFile на
`WMapData.cpp:504`, но он сработает и на Include-путях
(`OnIncludeClass` → `IncludeModelFromKey("MODEL", ...)`, `WRPGItem.cpp:203`).

## 5. Префикс класса и сопоставление с таблицей

### 5.1. Префикс

`GetResourceClassPrefix` (`WDataCore.cpp:2007-2014`):

```cpp
const char* CWorldDataCore::GetResourceClassPrefix(int _iRcClass)
    if (!m_lClasses.ValidPos(_iRcClass)) Error(...);
    return m_lClasses[_iRcClass].m_Prefix;
```

Регистрация (`WDataCore.cpp:756`):
`m_lClasses.Add(CWD_ResourceClassDesc("CWRes_Model_XMD", "XMD"));` →
префикс **`"XMD"`**, итоговая строка запроса `"XMD:<имя>"`. `m_Prefix` —
`char[4]`, копируется 3 символа (`WDataCore.h:83-96`).

Полный список префиксов модельных классов (enum `WDataRes_Core.h:27-69`,
регистрация `WDataCore.cpp:750-789`):

| Класс | Префикс | Строка регистрации |
|---|---|---|
| WRESOURCE_CLASS_MODEL_XW | `BSP` | :754 |
| WRESOURCE_CLASS_MODEL_XMD | `XMD` | :756 |
| WRESOURCE_CLASS_MODEL_CUSTOM | `CTM` | :760 |
| WRESOURCE_CLASS_MODEL_CUSTOM_FILE | `CMF` | :761 |
| WRESOURCE_CLASS_MODEL_XW2/XW3/XW4 | `XW2`/`XW3`/`XW4` | :775,:786,:787 |
| WRESOURCE_CLASS_MODEL_GLASS | `XGL` | :789 |

Отдельного класса `MODEL_SKEL` нет — персонажи идут тем же XMD (напр.
`WObj_CharMechanics.cpp:9613` — `GetResourceIndex_Model(
"Characters\\sever\\sever_head")`) или ветками CUSTOM/CMF.

Диспетчер `GetResourceIndex_Model` (`WMapData.cpp:500-548`): `.XW` в имени
(после `MakeUpperCase`, :506-510) → BSPModel; префикс до `:` совпал с
зарегистрированным классом `CXR_Model_*` (:516) → CTM/CMF;
`MultiTriMesh:...` (:524-529, :537-542) → `CMF:MultiTriMesh:...`; иначе →
XMD (:545).

### 5.2. Сопоставление строки с таблицей

- Нормализация на стороне запроса — `StrFixFilename`
  (`WDataCore.cpp:16-33`): `\` и `/` → одиночный `/`, регистр не трогается.
- Хэш — `CStringHash` (`MDA_Hash.h:228-241`, реализация
  `MDA_Hash.cpp:236-265`). Создаётся **case-insensitive** в обоих местах:
  `WMapData.cpp:376` и `WDataCore.cpp:2177` — `Create(Max(512,...), false)`.
  Сравнение (`MDA_Hash.cpp:256-259`): `Compare` при case-sensitive,
  `CompareNoCase` иначе.
- Класс по префиксу тоже регистронезависимо: `strnicmp(m_lClasses[i].m_Prefix,
  TmpName, 3)` (`WDataCore.cpp:2317-2319`).

**Расхождения регистра быть не может.** Подвох только по разделителям:
`StrFixFilename` применяется при lookup, а `CMapData::Hash_Insert`
(`WMapData.cpp:350-380`) кладёт `GetResourceName(iRc)` как есть. При
create-on-demand имя хранится уже нормализованным — консистентно. Но имена,
пришедшие из RESOURCES-записи/репликации с `\` внутри, lookup'ом с `/` не
совпадут. Для серверного пути предмета (create-on-demand) это не играет.

## 6. Сравнение с рабочим случаем (модель тела персонажа)

### 6.1. Тот же API

Ключ `MODEL` персонажа обрабатывает базовый `CWObject_Model::OnEvalKey`
(иерархия `CWObject_Character : CWObject_Player : ... : CWObject_Model`,
`WObj_Char.h:821`):

`Source/P5/Shared/MOS/Classes/GameWorld/WObjects/WObj_System.cpp:276-281`:

```cpp
case MHASH2('MODE','L'): // "MODEL"
case MHASH2('MODE','L0'): // "MODEL0"
	{
		Model_Set(0, _Value);
		break;
	}
```

`WObj_System.cpp:198-203` — тот же вызов, что и у предмета:

```cpp
void CWObject_Model::Model_Set(int _iPos, const char* _pName, bool _bAutoSetPhysics)
{
	Model_Set(_iPos, m_pWServer->GetMapData()->GetResourceIndex_Model(_pName), _bAutoSetPhysics);
}
```

У персонажа кейс переопределён, но делегирует вверх
(`WObj_CharCreate.cpp:1111-1121`: `CWObject_Player::OnEvalKey(...)` +
FaceSetup).

### 6.2. Три отличия пути предмета

1. **Момент вызова.** Персонаж спавнится из уровня внутри
   `CWorld_ServerCore::World_Change` — **до** `SetState(NOCREATE)`
   (`WServer_World.cpp:306`). Предмет создаётся через
   `CRPG_Object::CreateObject` при спавне персонажа
   (`WObj_CharCreate.cpp` ~:2717) или позже — при выдаче/подборе в геймплее.
2. **Прекэш смотрит на ДРУГОЙ ключ.** У персонажа `OnIncludeTemplate`
   (`WObj_CharCreate.cpp:625-665`: `IncludeModelFromKey("MODEL", ...)`
   для GIBPART и пр.) заносит имена в хэш заранее. У предмета
   `OnIncludeClass` (`WRPGItem.cpp:281-289`) прекэшит `ATTACHMODEL0..3` и
   `ATTACHMODEL`:

   ```cpp
   for(int i = 0; i < ATTACHMODEL_NUMMODELS; i++)
   {
       const CRegistry *pChild = _pReg->FindChild(CFStrF("ATTACHMODEL%i", i));
       if(pChild) { ... _pMapData->GetResourceIndex_Model(Str); }
   }
   ```

   а runtime читает `MODEL`. Т.е. даже своевременный прекэш имя модели
   предмета не покрывает (та же пара в `WRPGItem2.cpp:170` vs `:240`).
3. **Класс ресурса** — расхождений нет: оба пути без префикса сваливаются в
   ветку XMD (`WMapData.cpp:544-545`).

### 6.3. Репликация индекса server→client

Индекс реплицируется напрямую, клиент имя не резолвит.
`CAutoVar_AttachModel` (`WObj_AutoVar_AttachModel.h:336`):
`uint16 m_iModel[ATTACHMODEL_NUMMODELS]; // Replicated`. Pack/Unpack —
`WObj_AutoVar_AttachModel.cpp:266/321`:

```cpp
PTR_PUTINT16(_pD, m_iModel[i]);   // Pack, :278
...
PTR_GETINT16(_pD, m_iModel[i]);   // Unpack, :332
if(iOldModel != m_iModel[i]) {
	CXR_Model *pModel = _pMapData->GetResource_Model(m_iModel[i]);  // только по индексу
	... m_lspModelInstances[i] = pModel->CreateModelInstance();
```

`IsValid()` = `m_iModel[0] != 0` (`WObj_AutoVar_AttachModel.cpp:130-134`) —
ноль с сервера едет на клиент как есть. Экипировка копирует серверный
`m_Model` предмета в client-data: `WObj_CharMechanics.cpp:6005-6009` —
`pCD->m_Item0_Model.UpdateModel(pItem->m_Model); ... MakeDirty();`. Рендер
на клиенте: `WObj_CharRender.cpp:1412-1424` (`GetModel0_RenderInfo` →
`RenderExtraModels`). У тела персонажа `m_iModel[0]` реплицируется так же
побитово (`WObjCore.cpp:1329` — `PTR_PUTINT16(pD, m_iModel[0])`, распаковка
:1784 + `UpdateModelInstance`).

**Вывод:** поскольку зонд `[ITEM]` на клиенте видит `model=0`, а клиент
сам по имени ничего не ищет — ноль гарантированно рождён на сервере: либо
`case MODEL` не выполнялся (нет ключа в шаблоне), либо вернул 0 (только
NOCREATE/исключение, см. §3).

## 7. Побочное: `WObj_CharMechanics.cpp:6031` — подтверждено

В ветке `_iSlot != 0` все поля пишутся в `m_Item1_*`, кроме attachpoint:

```cpp
// WObj_CharMechanics.cpp:6026-6033
else
{
    //pCD->m_Item1_Model.SCopyFrom(...);
    pCD->m_Item1_Model.UpdateModel(pItem->m_Model);
    if (pItem->m_iFirstAttachPoint != -1)
        pCD->m_Item0_Model.m_iAttachPoint[0] = pItem->m_iFirstAttachPoint;  // <-- Item0 в ветке слота 1
    pCD->m_Item1_Model.MakeDirty();
    pCD->m_Item1_Flags = (uint8)pItem->m_Flags;
```

Симметричная ветка слота 0 (`:6003-6008`) пишет корректно в `m_Item0_Model`.
Это опечатка исходных авторов (код PS3-наследия, не артефакт порта). На баг
с `m_iModel[0]` не влияет: `UpdateModel` строкой выше уже скопировал
`m_iModel[]` целиком; трогает только `m_iAttachPoint[0]` и только при
`m_iFirstAttachPoint != -1`.

## 8. Следующий шаг (предложение, не фикс)

Одна строка зонда на `WRPGItem.cpp:355`: печатать `KeyValue` и результат
`GetResourceIndex_Model`. По наличию/отсутствию строки и значению индекса
гипотезы §1.4/§2.2 (нет ключа MODEL) и §3.2 (NOCREATE) разводятся
однозначно. Дополнительно имеет смысл распечатать сам вычисленный шаблон
оружия из `SERVER\RPGTEMPLATES` — какие ключи там реально есть.
