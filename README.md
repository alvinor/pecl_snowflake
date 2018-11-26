# snowflake

An implementation of twitter's snowflake with C , build in php extension. 

### Install
```
$ git clone https://github.com/alvinor/pecl_snowflake.git
$ cd  pecl_snowflake
$ phpize
$ ./configure 
$ make && make install
```

### Initial
```
extension = snowflake.so

```

### Functions

```
snowflake_id(int $work_node = 0)

$work_node: An uniq ID should between 0 and 1023 

```

### Demo 

```php
<?php
echo snowflake_id(123);

```

