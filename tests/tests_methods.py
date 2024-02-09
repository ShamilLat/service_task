import pytest

from testsuite.databases import pgsql

async def test_get_value(service_client):
    response = await service_client.get(
        '/todo/tasks',
        params={'user_ip': "127.0.0.1", 'notes_status': "uncomplete"}
    )
    assert response.status == 200
    
    response = await service_client.get(
        '/todo/tasks',
        params={'user_ip': "", 'notes_status': "uncomplete"}
    )
    assert response.status == 400
    
    response = await service_client.get(
        '/todo/tasks',
        params={'user_ip': "127.0.0.1", 'notes_status': "invalid_message"}
    )
    assert response.status == 400
    
    response = await service_client.get(
        '/todo/tasks',
        params={'user_ip': "127.0.0.1", 'notes_status': "any"}
    )
    assert response.status == 404


async def test_put_value(service_client):
    response = await service_client.put(
        '/todo/tasks',
        params={'user_ip': '127.0.0.1', 'note_text': 'Test note'}
    )
    assert response.status == 201
    
    response = await service_client.put(
        '/todo/tasks',
        params={'user_ip': '127.0.0.1', 'note_text': ''}
    )
    assert response.status == 400


async def test_patch_value(service_client):
    response = await service_client.patch(
        '/todo/tasks',
        params={'note_id': str(1), 'note_text': '', 'note_status': 'true'}
    )
    assert response.status == 400

    response = await service_client.patch(
        '/todo/tasks',
        params={'note_id': str(0), 'note_text': 'Note', 'note_status': 'invalid_status'}
    )
    assert response.status == 400

@pytest.mark.pgsql('db_1', files=['test_data.sql'])
async def test_db_initial_data(service_client):
    response = await service_client.patch(
        '/todo/tasks',
        params={'note_id': str(1), 'note_text': 'update text', 'note_status': 'true'},
    )
    assert response.status == 200