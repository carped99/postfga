-- Initialize postfga extension on startup
DO $$
DECLARE
    ext   text := 'postfga';
    cur   text := current_setting('shared_preload_libraries', true);
    norm  text;
    elems text[];
    new   text;
BEGIN
    -- 1) 현재 값이 없거나 비어 있으면 그냥 'postfga'로 설정
    IF cur IS NULL OR btrim(cur) = '' THEN
        new := ext;

    ELSE
        -- 2) 공백 제거해서 토큰 리스트 만들고, 이미 포함돼 있는지 검사
        norm  := replace(cur, ' ', '');
        elems := string_to_array(norm, ',');

        IF NOT ext = ANY (elems) THEN
            -- 3) 다른 값들만 있고 postfga가 없으면 뒤에 추가
            new := cur || ',postfga';
        ELSE
            -- 4) 이미 포함돼 있으면 기존 값 유지
            new := cur;
        END IF;
    END IF;

    -- 5) 클러스터 전체 설정으로 영구 반영
    EXECUTE format('ALTER SYSTEM SET shared_preload_libraries = %L',new);
END;
$$;;

-- Set GUC parameters
SET postfga.endpoint = 'dns:///openfga:8081';
SET postfga.store_id = 'test-store';
SET postfga.cache_ttl_ms = 60000;

-- Create extension
CREATE EXTENSION IF NOT EXISTS postfga;

-- Create test server
CREATE SERVER IF NOT EXISTS postfga_server
  FOREIGN DATA WRAPPER postfga_fdw
  OPTIONS (
    endpoint 'dns:///openfga:8081',
    store_id 'test-store'
  );

-- Create foreign table for testing
CREATE FOREIGN TABLE IF NOT EXISTS acl_permission (
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text
) SERVER openfga_server;

-- Grant permissions
GRANT SELECT ON acl_permission TO PUBLIC;
