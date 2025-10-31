import logging
import sqlite3
from requests import session
from sqlalchemy import create_engine, Column, Integer, String, Float, DateTime, ForeignKey, Text, Boolean, or_, event
from sqlalchemy.orm import sessionmaker, relationship, declarative_base, joinedload
from datetime import datetime, timedelta
from collections import defaultdict
from contextlib import contextmanager
from typing import Optional, Dict

# Configuração de Logging (apenas uma vez, idealmente no ponto de entrada da aplicação)
logging.basicConfig(level=logging.CRITICAL, format='%(asctime)s - %(levelname)s - %(message)s')

# Definir a base declarativa para os modelos
Base = declarative_base()

# --- Modelos SQLAlchemy (mapeando as tabelas do seu db.sql) ---
# Vamos mapear as tabelas mais cruciais para o handler base.
# Você pode estender isso para todas as tabelas conforme necessário.

class Area(Base):
    __tablename__ = 'area'
    id_area = Column(Integer, primary_key=True)
    nome = Column(Text, nullable=True)
    funcao = Column(Text, nullable=True)
    ativa_por_padrao = Column(Integer, nullable=True)
    memorias = relationship("Memoria", back_populates="area_rel")

class Pessoa(Base):
    __tablename__ = 'pessoas'
    id_pessoa = Column(Integer, primary_key=True)
    nome_pessoa = Column(Text, nullable=True)
    importancia = Column(Float, default=0.3, nullable=True)
    voz = Column(Text, nullable=True)
    foto = Column(Text, nullable=True)
    memorias = relationship("Memoria", back_populates="pessoa_rel")

class EstadoEmocional(Base):
    __tablename__ = 'estado_emocional'
    id_estado = Column(Integer, primary_key=True)
    nome = Column(Text, nullable=True)
    intensidade = Column(Float, default=1.0, nullable=True)
    memorias = relationship("Memoria", back_populates="estado_emocional_rel")

class Memoria(Base):
    __tablename__ = 'memorias'
    id_memorias = Column(Integer, primary_key=True)
    tipo = Column(Text, nullable=False)
    conteudo = Column(Text, nullable=False)
    contexto = Column(Text, nullable=False)
    emocao = Column(Text, nullable=False)
    tempo = Column(Text, nullable=True) # Armazenar como texto (ISO format) para SQLite DATETIME
    origem = Column(Text, nullable=False)
    id_area = Column(Integer, ForeignKey('area.id_area'), nullable=True)
    importancia = Column(Float, nullable=False)
    acessos = Column(Integer, default=0, nullable=True)
    vetor = Column(Text, nullable=True) # Para armazenar vetores de embedding como string (JSON ou comma-separated)
    id_estado_emocional = Column(Integer, ForeignKey('estado_emocional.id_estado'), nullable=True)
    id_pessoa = Column(Integer, ForeignKey('pessoas.id_pessoa'), nullable=True)
    decadencia = relationship("Decadencia", uselist=False, back_populates="memoria")
    timestamp = Column(DateTime, default=datetime.now)

    area_rel = relationship("Area", back_populates="memorias")
    pessoa_rel = relationship("Pessoa", back_populates="memorias")
    estado_emocional_rel = relationship("EstadoEmocional", back_populates="memorias")
    autoavaliacoes = relationship("AutoAvaliacao", back_populates="evidencia_memoria_rel")
    contextos = relationship("Contexto", back_populates="memoria_rel")
    emocoes = relationship("Emocao", back_populates="memoria_rel")
    entradas_sensor = relationship("EntradaSensor", back_populates="vinculado_a_rel")
    reflexoes = relationship("Reflexao", back_populates="memoria_rel")
    vinculos_origem = relationship("Vinculo", foreign_keys='Vinculo.origem_id', back_populates="origem_memoria_rel")
    vinculos_destino = relationship("Vinculo", foreign_keys='Vinculo.destino_id', back_populates="destino_memoria_rel")

    def to_dict(self):
        return {
            "timestamp": self.tempo,
            "text_input": self.conteudo,  # Ajuste conforme seu campo correto
            "contexto": self.contexto,
            "emocao": self.emocao,
            "tipo": self.tipo
        }

class AutoAvaliacao(Base):
    __tablename__ = 'autoavaliacoes'
    id_auto = Column(Integer, primary_key=True)
    tipo = Column(Text, nullable=True)
    descricao = Column(Text, nullable=True)
    evidencia_memoria_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    criado_em = Column(Text, nullable=True) # Armazenar como texto
    evidencia_memoria_rel = relationship("Memoria", back_populates="autoavaliacoes")

class Contexto(Base):
    __tablename__ = 'contextos'
    id_contextos = Column(Integer, primary_key=True)
    memoria_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    termo = Column(Text, nullable=True)
    memoria_rel = relationship("Memoria", back_populates="contextos")

class Emocao(Base):
    __tablename__ = 'emocoes'
    id_emocao = Column(Integer, primary_key=True)
    memoria_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    emocao = Column(Text, nullable=True)
    intensidade = Column(Float, default=1.0, nullable=True)
    memoria_rel = relationship("Memoria", back_populates="emocoes")

class EstadoEmocionalAtual(Base):
    __tablename__ = "estado_emocional_atual"

    id_estado = Column(Integer, ForeignKey("estado_emocional.id_estado"), primary_key=True, unique=False)
    atualizado_em = Column(DateTime)

class EntradaSensor(Base):
    __tablename__ = 'entradas_sensor'
    id_sensor = Column(Integer, primary_key=True)
    tipo = Column(Text, nullable=True)
    conteudo = Column(Text, nullable=True)
    tempo = Column(Text, nullable=True) # Armazenar como texto
    vinculado_a = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    vinculado_a_rel = relationship("Memoria", back_populates="entradas_sensor")

class FluxoPensamento(Base):
    __tablename__ = 'fluxo_pensamento'
    id_fluxo = Column(Integer, primary_key=True)
    id_origem = Column(Integer, nullable=True)
    id_destino = Column(Integer, nullable=True)
    tipo = Column(Text, nullable=True)
    intensidade = Column(Float, default=1.0, nullable=True)
    tempo = Column(Text, nullable=True) # Armazenar como texto

class IntencaoAtiva(Base):
    __tablename__ = 'intencao_ativa'
    id_intencao = Column(Integer, primary_key=True)
    descricao = Column(Text, nullable=True)
    tipo = Column(Text, nullable=True)
    gatilho = Column(Text, nullable=True)
    criado_em = Column(Text, nullable=True) # Armazenar como texto
    ativa = Column(Boolean, nullable=False, default=1)
    motivacao = Column(Float, default=0.0)

class MemoriaHandlerLog(Base):
    __tablename__ = 'memoria_handler_log'
    id_log = Column(Integer, primary_key=True)
    tipo_operacao = Column(Text, nullable=True)
    memoria_alvo = Column(Text, nullable=True)
    contexto_entrada = Column(Text, nullable=True)
    emocao_entrada = Column(Text, nullable=True)
    resultado = Column(Text, nullable=True)
    executado_em = Column(Text, nullable=True) # Armazenar como texto

class Modificacao(Base):
    __tablename__ = 'modificacoes'
    id_mod = Column(Integer, primary_key=True)
    alvo = Column(Text, nullable=True)
    descricao = Column(Text, nullable=True)
    motivacao = Column(Text, nullable=True)
    memoria_ref = Column(Integer, nullable=True)
    modificacoescol = Column(Text, nullable=True) # Parece ser um erro de digitação no schema original?

class Reflexao(Base):
    __tablename__ = 'reflexoes'
    id_reflexao = Column(Integer, primary_key=True)
    memoria_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    tipo = Column(Text, nullable=True)
    conteudo = Column(Text, nullable=True)
    criado_em = Column(Text, nullable=True) # Armazenar como texto
    memoria_rel = relationship("Memoria", back_populates="reflexoes")

class Vinculo(Base):
    __tablename__ = 'vinculos'
    id_vinculos = Column(Integer, primary_key=True)
    origem_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    destino_id = Column(Integer, ForeignKey('memorias.id_memorias'), nullable=True)
    peso = Column(Float, default=1.0, nullable=True)
    tipo = Column(Text, nullable=True)
    origem_memoria_rel = relationship("Memoria", foreign_keys=[origem_id], back_populates="vinculos_origem")
    destino_memoria_rel = relationship("Memoria", foreign_keys=[destino_id], back_populates="vinculos_destino")

class Decadencia(Base):
    __tablename__ = 'decadencia'

    id_memoria = Column(Integer, ForeignKey('memorias.id_memorias'), primary_key=True)
    ultimo_acesso = Column(DateTime)
    valor_atual = Column(Float)
    decaindo_desde = Column(DateTime)

    memoria = relationship("Memoria", back_populates="decadencia")

# --- Handler do Banco de Dados ---
class DatabaseHandler:
    def __init__(self, db_path="sqlite:///memory_system/memoria_db.sqlite"):
        """
        Inicializa o handler do banco de dados.
        :param db_path: Caminho para o arquivo SQLite. Ex: "sqlite:///memoria_db.sqlite"
        """
        self.engine = create_engine(db_path)
        Base.metadata.create_all(self.engine)  # Cria as tabelas se não existirem
        self.Session = sessionmaker(bind=self.engine, expire_on_commit=False)
        logging.info(f"Banco de dados SQLite inicializado em: {db_path}")
        self.DECAY_PER_HOUR = 0.01
        self.MIN_IMPORTANCIA = 0.3
        self.HARD_LOCK_IMPORTANCIA = 0.9

    def _get_session(self):
        """Retorna uma nova sessão do SQLAlchemy."""
        session = self.Session()
        return session

    def _log_operation(self, tipo_operacao: str, memoria_alvo: str = None, contexto_entrada: str = None,
                       emocao_entrada: str = None, resultado: str = None, current_session=None):
        """Registra uma operação no log do handler. Pode usar uma sessão existente."""
        session = current_session if current_session else self._get_session()
        close_session = (current_session is None)
        try:
            log_entry = MemoriaHandlerLog(
                tipo_operacao=tipo_operacao,
                memoria_alvo=memoria_alvo,
                contexto_entrada=contexto_entrada,
                emocao_entrada=emocao_entrada,
                resultado=resultado,
                executado_em=datetime.now().isoformat()
            )
            session.add(log_entry)
            if close_session: # Only commit if we opened the session here
                session.commit()
        except Exception as e:
            if close_session:
                session.rollback()
            logging.error(f"Erro ao registrar log: {e}")
        finally:
            if close_session: # Only close if we opened the session here
                session.close()

    # --- Métodos para a tabela Area ---
    def create_area(self, nome: str, funcao: str = None, ativa_por_padrao: int = 0) -> int | None:
        """Cria uma nova área e retorna seu ID."""
        session = self._get_session()
        try:
            new_area = Area(nome=nome, funcao=funcao, ativa_por_padrao=ativa_por_padrao)
            session.add(new_area)
            session.commit()
            area_id = new_area.id_area
            self._log_operation("CREATE_AREA", memoria_alvo=f"Area ID:{area_id}", resultado="Sucesso", current_session=session)
            return area_id
        except Exception as e:
            session.rollback()
            self._log_operation("CREATE_AREA", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao criar área: {e}")
            return None
        finally:
            session.close()

    def get_area_by_name(self, nome: str) -> Area | None:
        """Recupera uma área pelo nome."""
        session = self._get_session()
        try:
            area = session.query(Area).filter_by(nome=nome).first()
            return area
        finally:
            session.close()

    # --- Métodos para a tabela EstadoEmocional ---
    def create_estado_emocional(self, nome: str, intensidade: float = 1.0) -> int | None:
        """Cria um novo estado emocional e retorna seu ID."""
        session = self._get_session()
        try:
            new_estado = EstadoEmocional(nome=nome, intensidade=intensidade)
            session.add(new_estado)
            session.commit()
            estado_id = new_estado.id_estado
            self._log_operation("CREATE_ESTADO_EMOCIONAL", resultado="Sucesso", current_session=session)
            return estado_id
        except Exception as e:
            session.rollback()
            self._log_operation("CREATE_ESTADO_EMOCIONAL", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao criar estado emocional: {e}")
            return None
        finally:
            session.close()

    def get_estado_emocional_by_name(self, nome: str) -> EstadoEmocional | None:
        """Recupera um estado emocional pelo nome."""
        session = self._get_session()
        try:
            estado = session.query(EstadoEmocional).filter_by(nome=nome).first()
            return estado
        finally:
            session.close()

    # --- Operações de Memória ---
    def create_memory(self, tipo: str, conteudo: str, contexto: str, emocao: str, origem: str, importancia: float,
                      id_area: int = None, id_pessoa: int = None, id_estado_emocional: int = None) -> Memoria | None:
        """Cria uma nova memória."""
        session = self._get_session()
        try:
            new_memory = Memoria(
                tipo=tipo,
                conteudo=conteudo,
                contexto=contexto,
                emocao=emocao,
                tempo=datetime.now().isoformat(),
                origem=origem,
                importancia=importancia,
                id_area=id_area,
                id_pessoa=id_pessoa,
                id_estado_emocional=id_estado_emocional
            )
            session.add(new_memory)
            session.commit()
            session.refresh(new_memory)
            decadencia = Decadencia(
                id_memoria=new_memory.id_memorias,
                ultimo_acesso=datetime.now(),
                valor_atual=new_memory.importancia,
                decaindo_desde=datetime.now()
            )
            session.add(decadencia)
            session.commit()
            self._log_operation("CREATE_MEMORY", memoria_alvo=f"ID:{new_memory.id_memorias}",
                                contexto_entrada=contexto, emocao_entrada=emocao, resultado="Sucesso", current_session=session)
            return new_memory
        except Exception as e:
            session.rollback()
            self._log_operation("CREATE_MEMORY", contexto_entrada=contexto, emocao_entrada=emocao,
                                resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao criar memória: {e}")
            return None
        finally:
            session.close()

    def get_memory_by_id(self, memory_id: int) -> Memoria | None:
        """
        Recupera uma memória pelo ID, carregando ansiosamente os relacionamentos.
        """
        session = self._get_session()
        try:
            memoria = session.query(Memoria).options(
                joinedload(Memoria.pessoa_rel),
                joinedload(Memoria.area_rel),
                joinedload(Memoria.estado_emocional_rel)
            ).get(memory_id)
            self._log_operation("GET_MEMORY_BY_ID", memoria_alvo=f"ID:{memory_id}",
                                resultado="Sucesso" if memoria else "Não Encontrado", current_session=session)
            return memoria
        except Exception as e:
            self._log_operation("GET_MEMORY_BY_ID", memoria_alvo=f"ID:{memory_id}", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao obter memória por ID: {e}")
            return None
        finally:
            session.close()

    def update_memory(self, memory_id: int, updates: dict) -> bool:
        """Atualiza campos de uma memória existente."""
        session = self._get_session()
        try:
            memoria = session.query(Memoria).get(memory_id)
            if memoria:
                for key, value in updates.items():
                    if hasattr(memoria, key):
                        setattr(memoria, key, value)
                session.commit()
                self._log_operation("UPDATE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado="Sucesso", current_session=session)
                return True
            self._log_operation("UPDATE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado="Não Encontrado", current_session=session)
            return False
        except Exception as e:
            session.rollback()
            self._log_operation("UPDATE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao atualizar memória: {e}")
            return False
        finally:
            session.close()

    def delete_memory(self, memory_id: int) -> bool:
        """Deleta uma memória e suas entradas dependentes (se configurado em CASCADE)."""
        session = self._get_session()
        try:
            memoria = session.query(Memoria).get(memory_id)
            if memoria:
                session.delete(memoria)
                session.commit()
                self._log_operation("DELETE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado="Sucesso", current_session=session)
                return True
            self._log_operation("DELETE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado="Não Encontrado", current_session=session)
            return False
        except Exception as e:
            session.rollback()
            self._log_operation("DELETE_MEMORY", memoria_alvo=f"ID:{memory_id}", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao deletar memória: {e}")
            return False
        finally:
            session.close()

    # --- Operações de Busca (com integração nanoGPT conceitual) ---
    def search_memories(self, query_params: dict, limit: int = 20) -> list[Memoria]:
        """
        Busca memórias com base em parâmetros.
        Estes parâmetros podem ser pré-processados por um nanoGPT.
        Ex: query_params = {"conteudo_like": "%cachorro%", "tipo": "evento"}
        """
        session = self._get_session()
        try:
            q = session.query(Memoria)
            for key, value in query_params.items():
                if key == "conteudo_like":
                    q = q.filter(Memoria.conteudo.like(value))
                elif key == "contexto_like":
                    q = q.filter(Memoria.contexto.like(value))
                elif key == "emocao":
                    q = q.filter(Memoria.emocao == value)
                elif key == "tipo":
                    q = q.filter(Memoria.tipo == value)
                elif key == "id_pessoa":
                    q = q.filter(Memoria.id_pessoa == value)
                # Adicione mais filtros conforme necessário
            results = q.limit(limit).all()
            self._log_operation("SEARCH_MEMORIES", contexto_entrada=str(query_params),
                                resultado=f"Sucesso, {len(results)} encontrados", current_session=session)
            return results
        except Exception as e:
            self._log_operation("SEARCH_MEMORIES", contexto_entrada=str(query_params), resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao buscar memórias: {e}")
            return []
        finally:
            session.close()

    def search_recent_memories(self, limit=10, hours_ago=24) -> list:
        """Busca memórias recentes dentro de um intervalo de horas."""
        session = self._get_session()
        try:
            cutoff_time = datetime.now() - timedelta(hours=hours_ago)
            results = session.query(Memoria).filter(Memoria.tempo >= cutoff_time.isoformat()).order_by(Memoria.tempo.desc()).limit(limit).all()

            # Convert SQLAlchemy objects to a list of dictionaries
            python_list_results = [memoria.to_dict() for memoria in results]

            self._log_operation("SEARCH_RECENT_MEMORIES", contexto_entrada=f"Últimas {hours_ago} horas",
                                resultado=f"Sucesso, {len(python_list_results)} encontrados", current_session=session)
            return python_list_results
        except Exception as e:
            self._log_operation("SEARCH_RECENT_MEMORIES", contexto_entrada=f"Últimas {hours_ago} horas", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao buscar memórias recentes: {e}")
            return []
        finally:
            session.close()

    def semantic_search_memories(self, query_embedding_str: str, top_k: int = 5) -> list[Memoria]:
        """
        Realiza uma busca semântica por similaridade de vetor.
        Aqui, o `query_embedding_str` seria gerado pelo nanoGPT ou outro modelo de embedding.
        IMPORTANTE: SQLite não tem suporte nativo a busca vetorial eficiente.
        Esta função faria uma comparação vetorial simples, que pode ser lenta
        para muitos registros. Para escala, considere integrar com FAISS/Annoy.
        """
        try:
            query_embedding = [float(x) for x in query_embedding_str.split(',')]
        except ValueError:
            logging.warning("Vetor de consulta inválido.")
            return []

        session = self._get_session()
        all_memories_with_vectors = []
        try:
            all_memories_with_vectors = session.query(Memoria).filter(Memoria.vetor.isnot(None)).all()
        finally:
            session.close() # Close session after initial query

        if not all_memories_with_vectors:
            self._log_operation("SEMANTIC_SEARCH", contexto_entrada="Sem vetores para busca", resultado="Vazio")
            return []

        similarities = []
        for mem in all_memories_with_vectors:
            try:
                mem_vector = [float(x) for x in mem.vetor.split(',')]
                dot_product = sum(a*b for a,b in zip(query_embedding, mem_vector))
                norm_query = sum(a*a for a in query_embedding)**0.5
                norm_mem = sum(b*b for b in mem_vector)**0.5

                if norm_query == 0 or norm_mem == 0:
                    similarity = 0.0
                else:
                    similarity = dot_product / (norm_query * norm_mem)

                similarities.append((similarity, mem))
            except ValueError:
                continue

        similarities.sort(key=lambda x: x[0], reverse=True)
        results = [mem for sim, mem in similarities[:top_k]]

        self._log_operation("SEMANTIC_SEARCH", contexto_entrada=query_embedding_str,
                            resultado=f"Sucesso, {len(results)} encontrados")
        return results

    # --- Operações de Pessoas ---
    def create_person(self, nome_pessoa: str, importancia: float = 0.3, voz: str = None, foto: str = None) -> int | None:
        """Cria uma nova pessoa e retorna seu ID."""
        session = self._get_session()
        try:
            new_person = Pessoa(nome_pessoa=nome_pessoa, importancia=importancia, voz=voz, foto=foto)
            session.add(new_person)
            session.commit()
            person_id = new_person.id_pessoa
            self._log_operation("CREATE_PERSON", memoria_alvo=f"Pessoa ID:{person_id}", resultado="Sucesso", current_session=session)
            return person_id
        except Exception as e:
            session.rollback()
            self._log_operation("CREATE_PERSON", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao criar pessoa: {e}")
            return None
        finally:
            session.close()

    def get_person_by_id(self, person_id: int) -> Pessoa | None:
        """Recupera uma pessoa pelo ID."""
        session = self._get_session()
        try:
            person = session.query(Pessoa).get(person_id)
            self._log_operation("GET_PERSON_BY_ID", memoria_alvo=f"Pessoa ID:{person_id}",
                                resultado="Sucesso" if person else "Não Encontrado", current_session=session)
            return person
        except Exception as e:
            self._log_operation("GET_PERSON_BY_ID", memoria_alvo=f"Pessoa ID:{person_id}", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao obter pessoa por ID: {e}")
            return None
        finally:
            session.close()

    def get_person_by_name(self, nome_pessoa: str) -> Pessoa | None:
        """Recupera uma pessoa pelo nome."""
        session = self._get_session()
        try:
            person = session.query(Pessoa).filter_by(nome_pessoa=nome_pessoa).first()
            return person
        finally:
            session.close()

    # --- Operações de Emoções ---
    def add_emotion_to_memory(self, memory_id: int, emocao: str, intensidade: float = 1.0) -> tuple[int, str] | None:
        """Adiciona uma emoção a uma memória e retorna o ID e o nome da emoção."""
        session = self._get_session()
        try:
            new_emotion = Emocao(memoria_id=memory_id, emocao=emocao, intensidade=intensidade)
            session.add(new_emotion)
            session.commit()
            self._log_operation("ADD_EMOTION", memoria_alvo=f"Memória ID:{memory_id}",
                                contexto_entrada=emocao, resultado="Sucesso", current_session=session)
            return new_emotion.id_emocao, new_emotion.emocao
        except Exception as e:
            session.rollback()
            self._log_operation("ADD_EMOTION", memoria_alvo=f"Memória ID:{memory_id}",
                                contexto_entrada=emocao, resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao adicionar emoção: {e}")
            return None
        finally:
            session.close()

    def get_emotions_for_memory(self, memory_id: int) -> list[Emocao]:
        """Recupera todas as emoções para uma memória específica."""
        session = self._get_session()
        try:
            emotions = session.query(Emocao).filter_by(memoria_id=memory_id).all()
            self._log_operation("GET_EMOTIONS_FOR_MEMORY", memoria_alvo=f"Memória ID:{memory_id}",
                                resultado=f"Sucesso, {len(emotions)} encontrados", current_session=session)
            return emotions
        except Exception as e:
            self._log_operation("GET_EMOTIONS_FOR_MEMORY", memoria_alvo=f"Memória ID:{memory_id}",
                                resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao obter emoções para memória: {e}")
            return []
        finally:
            session.close()

    # --- Operações de Vinculos ---
    def create_link(self, origem_id: int, destino_id: int, peso: float = 1.0, tipo: str = None) -> int | None:
        """Cria um vínculo entre duas memórias e retorna o ID do vínculo."""
        session = self._get_session()
        try:
            new_link = Vinculo(origem_id=origem_id, destino_id=destino_id, peso=peso, tipo=tipo)
            session.add(new_link)
            session.commit()
            self._log_operation("CREATE_LINK", memoria_alvo=f"Origem:{origem_id}, Destino:{destino_id}",
                                resultado="Sucesso", current_session=session)
            return new_link.id_vinculos
        except Exception as e:
            session.rollback()
            self._log_operation("CREATE_LINK", memoria_alvo=f"Origem:{origem_id}, Destino:{destino_id}",
                                resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao criar vínculo: {e}")
            return None
        finally:
            session.close()

    def get_links_from_memory(self, memory_id: int) -> list[Vinculo]:
        """Obtém todos os vínculos onde a memória é a origem ou o destino."""
        session = self._get_session()
        try:
            links = session.query(Vinculo).filter(
                (Vinculo.origem_id == memory_id) | (Vinculo.destino_id == memory_id)
            ).all()
            self._log_operation("GET_LINKS_FROM_MEMORY", memoria_alvo=f"Memória ID:{memory_id}",
                                resultado=f"Sucesso, {len(links)} encontrados", current_session=session)
            return links
        except Exception as e:
            self._log_operation("GET_LINKS_FROM_MEMORY", memoria_alvo=f"Memória ID:{memory_id}",
                                resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao obter vínculos: {e}")
            return []
        finally:
            session.close()

    # --- Outras Operações (exemplos) ---
    def record_sensor_input(self, tipo: str, conteudo: str, vinculado_a: int = None) -> tuple[int, str] | None:
        """Registra uma entrada de sensor e retorna o ID e o conteúdo."""
        session = self._get_session()
        try:
            new_entry = EntradaSensor(tipo=tipo, conteudo=conteudo, tempo=datetime.now().isoformat(), vinculado_a=vinculado_a)
            session.add(new_entry)
            session.commit()
            self._log_operation("RECORD_SENSOR_INPUT", memoria_alvo=f"Vínculo:{vinculado_a}",
                                contexto_entrada=conteudo, resultado="Sucesso", current_session=session)
            return new_entry.id_sensor, new_entry.conteudo
        except Exception as e:
            session.rollback()
            self._log_operation("RECORD_SENSOR_INPUT", memoria_alvo=f"Vínculo:{vinculado_a}",
                                contexto_entrada=conteudo, resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao registrar entrada de sensor: {e}")
            return None
        finally:
            session.close()

    def decair_memorias(self):
        session = self._get_session()
        try:
            agora = datetime.now()
            mems = session.query(Decadencia).join(Memoria).filter(Memoria.importancia < self.HARD_LOCK_IMPORTANCIA).all()

            logging.info(f"🔍 Verificando {len(mems)} memórias para decaimento...")

            for d in mems:
                if not d.ultimo_acesso:
                    continue

                horas = (agora - d.ultimo_acesso).total_seconds() / 3600
                decay = horas * self.DECAY_PER_HOUR
                novo_valor = max(0.0, d.valor_atual - decay)

                if novo_valor < self.MIN_IMPORTANCIA:
                    logging.info(f"⚠️ Memória ID {d.id_memoria} está se tornando vaga. Valor: {novo_valor:.2f}")
                else:
                    logging.info(f"🧠 Memória ID {d.id_memoria} decaiu para {novo_valor:.2f}")

                d.valor_atual = novo_valor
                d.ultimo_acesso = agora

            session.commit()
            self._log_operation("DECAY_MEMORIES", resultado=f"{len(mems)} processadas", current_session=session)
        except Exception as e:
            session.rollback()
            logging.error(f"Erro ao decair memórias: {e}")
        finally:
            session.close()


    def registrar_estado_emocional_atual(self, id_estado, session=None):
        close_session = False
        if session is None:
            session = self._get_session()
            close_session = True
        try:
            agora = datetime.now()
            # Verifica se já existe um registro para esse id_estado
            existente = session.query(EstadoEmocionalAtual).filter_by(id_estado=id_estado).first()
            if existente:
                existente.atualizado_em = agora
                logging.info(f"🧠 Estado emocional atual atualizado: ID {id_estado} em {agora}")
            else:
                novo_estado = EstadoEmocionalAtual(
                    id_estado=id_estado,
                    atualizado_em=agora
                )
                session.add(novo_estado)
                logging.info(f"🧠 Estado emocional atual registrado: ID {id_estado} em {agora}")
            session.commit()
            self._log_operation("REGISTRO_ESTADO_ATUAL", resultado=f"id_estado={id_estado}", current_session=session)
        except Exception as e:
            if close_session:
                session.rollback()
            self._log_operation("REGISTRO_ESTADO_ATUAL_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao registrar estado emocional atual: {e}")
        finally:
            if close_session:
                session.close()

    def get_estado_emocional_atual(self, session=None):
        close_session = False
        if session is None:
            session = self.Session()
            close_session = True

        try:
            atual = session.query(EstadoEmocionalAtual).order_by(EstadoEmocionalAtual.atualizado_em.desc()).first()
            if not atual:
                logging.warning("⚠️ Nenhum estado emocional atual encontrado.")
                return None

            estado = session.query(EstadoEmocional).filter_by(id_estado=atual.id_estado).first()
            if not estado:
                logging.warning("⚠️ Emoção do estado atual não encontrada.")
                return None

            estado_dict = {
                "nome": estado.nome,
                "intensidade": estado.intensidade
            }
            return estado_dict

        finally:
            if close_session:
                session.close()

    def identificar_emocao_dominante(self, estado_emocional_dict=None, session=None):
        """
        Retorna a emoção dominante (chave com maior valor) e seu valor.
        """
        if not estado_emocional_dict:
            estado_emocional_dict = self.get_estado_emocional_atual(session=session)

        if not estado_emocional_dict:
            logging.warning("⚠️ Estado emocional ausente ou inválido.")
            return None, 0.0

        dominante_nome = estado_emocional_dict.get("nome")
        dominante_intensidade = estado_emocional_dict.get("intensidade")

        if dominante_nome is None or dominante_intensidade is None:
            logging.warning("⚠️ Estado emocional com formato inesperado.")
            return None, 0.0

        logging.info(f"🎭 Emoção dominante: {dominante_nome} (valor: {dominante_intensidade:.2f})")
        return dominante_nome, dominante_intensidade

    def obter_ultima_interacao(self, session=None):
        """
        Recupera a última interação registrada no banco de dados.
        """
        close_session = False
        if session is None:
            session = self.Session()
            close_session = True
        try:
            interacao = (
                session.query(Memoria)
                .order_by(Memoria.timestamp.desc())
                .first()
            )
            if interacao:
                logging.info(f"🕒 Última interação em: {interacao.timestamp}")
            return interacao
        except Exception as e:
            logging.error(f"❌ Erro ao buscar última interação: {e}")
            return None
        finally:
            if close_session:
                session.close()

    def ativar_intencao(self, descricao: str, tipo: str, gatilho: str = None, boost: float = 0.0) -> Optional[int]:
        """
        Cria ou reativa uma intenção com base em descrição, tipo e gatilho.
        Se já existir ativa, apenas retorna o ID. Se existir inativa, reativa.
        """
        session = self._get_session()
        try:
            intencao_existente = session.query(IntencaoAtiva).filter_by(
                descricao=descricao, tipo=tipo, gatilho=gatilho
            ).first()

            if intencao_existente:
                if not intencao_existente.ativa:
                    intencao_existente.ativa = True
                    intencao_existente.criado_em = datetime.now().isoformat()
                    intencao_existente.motivacao = boost
                    session.commit()
                    logging.info(f"🔄 Intenção reativada: {descricao}")
                else:
                    logging.info(f"ℹ️ Intenção já ativa: {descricao}")
                self._log_operation("ATIVAR_INTENCAO", resultado="Sucesso", memoria_alvo=descricao, current_session=session)
                return intencao_existente.id_intencao

            nova_intencao = IntencaoAtiva(
                descricao=descricao,
                tipo=tipo,
                gatilho=gatilho,
                criado_em=datetime.now().isoformat(),
                ativa=True,
                motivacao=boost
            )
            session.add(nova_intencao)
            session.commit()
            logging.info(f"🎯 Nova intenção ativada: {descricao}")
            self._log_operation("ATIVAR_INTENCAO", resultado="Sucesso", memoria_alvo=descricao, current_session=session)
            return nova_intencao.id_intencao

        except Exception as e:
            session.rollback()
            self._log_operation("ATIVAR_INTENCAO_FAILED", resultado=f"Falha: {e}", memoria_alvo=descricao, current_session=session)
            logging.error(f"❌ Erro ao ativar intenção '{descricao}': {e}")
            return None
        finally:
            session.close()

    def verificar_auto_ativacao(self):
        session = self._get_session()
        try:
            ultima_interacao = self.obter_ultima_interacao(session=session)
            if not ultima_interacao:
                logging.warning("⚠️ Nenhuma interação registrada. Autoativação cancelada.")
                return

            estado_atual = self.get_estado_emocional_atual(session=session)
            if not estado_atual:
                estado_atual = EstadoEmocional(nome="neutro", intensidade=0.1)
                logging.warning("⚠️ Estado emocional atual não encontrado.")
                return

            dominante, _ = self.identificar_emocao_dominante(estado_atual, session=session)

            intencoes_ativas = self.obter_intencoes_ativas(session=session)
            nomes_ativas = [i.descricao for i in intencoes_ativas]

            mapa_emocao_para_intencao = {
                "tristeza": "Refletir sobre sentimentos difíceis",
                "alegria": "Manter momentos positivos",
                "raiva": "Redirecionar frustrações",
                "medo": "Buscar segurança",
                "surpresa": "Explorar novidade",
                "confiança": "Apoiar outros",
                "desgosto": "Isolar influências negativas",
                "antecipacao": "Planejar próximos passos"
            }

            nome_intencao = mapa_emocao_para_intencao.get(dominante.lower())
            if nome_intencao and nome_intencao not in nomes_ativas:
                gatilho = f"emoção dominante:'{dominante.lower()}'"
                # Call self.ativar_intencao which gets its own session
                self.ativar_intencao(descricao=nome_intencao, tipo="emocional", gatilho=gatilho, boost=1.0)
                logging.info(f"🎯 Intenção '{nome_intencao}' ativada automaticamente com base na emoção '{dominante}'.")
                return

            texto_entrada = (ultima_interacao.conteudo or "").lower()
            padroes_para_intencao = {
                "não consigo parar de pensar": "Processar pensamentos repetitivos",
                "sinto que estou travada": "Romper bloqueios internos",
                "parece que estou esquecendo algo": "Refletir sobre memórias recentes",
                "quero aprender mais sobre": "Aprender sobre tópico emergente",
                "preciso entender melhor": "Investigar novo conceito"
            }

            for padrao, nome_intencao_textual in padroes_para_intencao.items():
                if padrao in texto_entrada and nome_intencao_textual not in nomes_ativas:
                    # Call self.ativar_intencao which gets its own session
                    self.ativar_intencao(descricao=nome_intencao_textual, tipo="aprendizado", gatilho=f"entrada:'{padrao}'")
                    logging.info(f"🎯 Intenção '{nome_intencao_textual}' ativada automaticamente com base no texto de entrada.")
                    return

            logging.info("ℹ️ Nenhuma intenção foi ativada automaticamente.")
            self._log_operation("VERIFICAR_AUTO_ATIVACAO", resultado="Concluído", current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("VERIFICAR_AUTO_ATIVACAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro em verificar_auto_ativacao: {e}")
        finally:
            session.close()

    def gerar_reflexoes(self):
        session = self._get_session()
        try:
            agora = datetime.now()
            limite_tempo = agora - timedelta(hours=6)
            cooldown = agora - timedelta(hours=6)

            mems = session.query(Memoria).join(Emocao).filter(
                Memoria.tempo >= limite_tempo.isoformat(),
                Memoria.importancia >= 0.5
            ).all()

            if not mems:
                logging.warning("⚠️ Nenhuma memória emocional recente encontrada para reflexão.")
                return

            estado_actual_data = self.get_estado_emocional_atual(session=session)

            if not estado_actual_data:
                logging.warning("⚠️ Nenhum estado emocional atual encontrado. Reflexões ignoradas.")
                return

            emocao_atual_nome = estado_actual_data.get("nome")
            if not emocao_atual_nome:
                logging.warning("⚠️ Emoção do estado atual não encontrada.")
                return

            grupos = defaultdict(list)
            for mem in mems:
                for emo in mem.emocoes:
                    grupos[emo.emocao].append(mem)

            reflexoes_criadas = 0

            for emocao, lista_mem in grupos.items():
                if len(lista_mem) >= 3:
                    logging.info(f"💡 Emoção recorrente detectada: {emocao} ({len(lista_mem)} vezes)")

                    conteudo = f"Notei que me senti muito {emocao.lower()} nas últimas horas. Talvez isso signifique algo sobre mim."
                    memoria_base = lista_mem[0]

                    reflexao_existente = session.query(Reflexao).filter(
                        Reflexao.tipo == "emocao",
                        Reflexao.memoria_id == memoria_base.id_memorias,
                        Reflexao.conteudo == conteudo,
                        Reflexao.criado_em >= cooldown.isoformat()
                    ).first()

                    if reflexao_existente:
                        logging.info(f"🧊 Reflexão recente já existe para emoção {emocao}. Ignorando.")
                        continue

                    nova = Reflexao(
                        memoria_id = memoria_base.id_memorias,
                        tipo = "emocao",
                        conteudo = conteudo,
                        criado_em = agora.isoformat()
                    )
                    session.add(nova)

                    nova_emocao = Emocao(
                        memoria_id = memoria_base.id_memorias,
                        emocao = emocao_atual_nome,
                        intensidade = 0.3
                    )
                    session.add(nova_emocao)

                    logging.info(f"🧠 Reflexão criada: {conteudo} (com emoção leve: {emocao_atual_nome})")
                    reflexoes_criadas += 1

            session.commit()
            self._log_operation("REFLEXOES", resultado=f"{reflexoes_criadas} geradas", current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("REFLEXOES_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao gerar reflexões: {e}")
        finally:
            session.close()

    def definir_intencao_ativa(self, descricao, tipo, gatilho="contexto"):
        session = self._get_session()
        try:
            nova = IntencaoAtiva(
                descricao=descricao,
                tipo=tipo,
                gatilho=gatilho,
                criado_em=datetime.now().isoformat(),
                ativa=True
            )
            session.add(nova)
            session.commit()
            logging.info(f"🎯 Nova intenção ativada: {descricao}")
            self._log_operation("INTENCAO_ATIVA", resultado=descricao, current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("INTENCAO_ATIVA_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao definir intenção ativa: {e}")
        finally:
            session.close()

    def obter_intencoes_ativas(self, session=None):
        close_session = False
        if session is None:
            session = self._get_session()
            close_session = True
        try:
            intencoes = session.query(IntencaoAtiva).filter_by(ativa=True).all()
            logging.info(f"🧭 Intenções ativas ({len(intencoes)}):")
            for i in intencoes:
                logging.info(f"  • [{i.id_intencao}] {i.descricao} ({i.tipo}) - Gatilho: {i.gatilho}")
            self._log_operation("GET_INTENCOES_ATIVAS", resultado=f"{len(intencoes)} encontradas", current_session=session)
            return intencoes
        except Exception as e:
            self._log_operation("GET_INTENCOES_ATIVAS_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao obter intenções ativas: {e}")
            return []
        finally:
            if close_session:
                session.close()

    def encerrar_intencao(self, id_intencao):
        session = self._get_session()
        try:
            intencao = session.query(IntencaoAtiva).get(id_intencao)
            if intencao:
                intencao.ativa = False
                session.commit()
                logging.info(f"🛑 Intenção encerrada: {intencao.descricao}")
                self._log_operation("ENCERRAR_INTENCAO", resultado=f"{id_intencao} encerrada", current_session=session)
            else:
                logging.warning(f"Intenção com ID {id_intencao} não encontrada para encerramento.")
        except Exception as e:
            session.rollback()
            self._log_operation("ENCERRAR_INTENCAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao encerrar intenção: {e}")
        finally:
            session.close()

    def buscar_memorias_com_intencao(self, termo):
        session = self._get_session()
        try:
            termo = termo.lower()
            intencoes = self.obter_intencoes_ativas(session=session) # Pass session
            boost_termos = [i.descricao.lower() for i in intencoes]

            mems = session.query(Memoria).all()
            resultados = []

            for m in mems:
                score = 0.0

                if termo in m.conteudo.lower():
                    score += 1.0

                for bt in boost_termos:
                    if bt in m.conteudo.lower() or bt in m.contexto.lower():
                        score += 0.5

                if score > 0:
                    resultados.append((m, score))

            resultados.sort(key=lambda x: x[1], reverse=True)

            logging.info(f"🔍 Memórias relevantes com boost (termo='{termo}'):")
            for m, s in resultados:
                logging.info(f"  • [{m.id_memorias}] {m.conteudo[:50]}... (score: {s:.1f})")

            self._log_operation("BUSCA_INTENCAO", resultado=f"{len(resultados)} encontradas", current_session=session)
            return [m for m, _ in resultados]
        except Exception as e:
            session.rollback()
            self._log_operation("BUSCA_INTENCAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao buscar memórias com intenção: {e}")
            return []
        finally:
            session.close()

    def analisar_entrada_para_intencao(self, entrada_usuario):
        session = self._get_session()
        try:
            entrada = entrada_usuario.lower()

            gatilhos = {
                "sensor": ("Aprender sobre sensores", "aprendizado"),
                "tato": ("Entender o tato artificial", "aprendizado"),
                "deyvid": ("Compreender melhor o Deyvid", "relacional"),
                "frustração": ("Lidar melhor com falhas", "autocuidado"),
                "feliz": ("Manter momentos positivos", "emocional"),
            }

            for palavra, (descricao, tipo) in gatilhos.items():
                if palavra in entrada:
                    ja_existe = session.query(IntencaoAtiva).filter_by(
                        descricao=descricao,
                        tipo=tipo,
                        ativa=True
                    ).first()
                    if not ja_existe:
                        self.definir_intencao_ativa(descricao, tipo, gatilho=f"entrada:'{palavra}'")
                        logging.info(f"🎯 Intenção '{descricao}' ativada por análise de entrada.")
                        self._log_operation("ANALISAR_ENTRADA_INTENCAO", resultado=f"Ativou: {descricao}", current_session=session)
                        return
                    else:
                        logging.info(f"🔁 Intenção '{descricao}' já estava ativa.")
                        self._log_operation("ANALISAR_ENTRADA_INTENCAO", resultado=f"Já ativa: {descricao}", current_session=session)
                        return
            self._log_operation("ANALISAR_ENTRADA_INTENCAO", resultado="Nenhuma intenção ativada", current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("ANALISAR_ENTRADA_INTENCAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao analisar entrada para intenção: {e}")
        finally:
            session.close()

    def avaliar_satisfacao_intencoes(self):
        session = self._get_session()
        try:
            agora = datetime.now()
            intencoes = self.obter_intencoes_ativas(session=session)

            for i in intencoes:
                try:
                    criado_em_dt = datetime.strptime(i.criado_em, "%Y-%m-%d %H:%M:%S.%f")
                except ValueError:
                    try:
                        criado_em_dt = datetime.fromisoformat(i.criado_em)
                    except ValueError:
                        logging.warning(f"⚠️ Aviso: Não foi possível analisar a string de data/hora '{i.criado_em}'. Pulando intenção.")
                        self._log_operation("ERROR_DATETIME_PARSE", resultado=f"Formato inválido para {i.descricao}", current_session=session)
                        continue

                idade = agora - criado_em_dt
                idade_horas = idade.total_seconds() / 3600

                mems_recentes = session.query(Memoria).filter(
                    or_(
                        Memoria.conteudo.ilike(f"%{i.descricao}%"),
                        Memoria.contexto.ilike(f"%{i.descricao}%")
                    ),
                    Memoria.tempo >= criado_em_dt.isoformat()
                ).all()

                if mems_recentes and idade_horas <= 2:
                    logging.info(f"✅ Intenção '{i.descricao}' sendo satisfeita ({len(mems_recentes)} memórias)")
                    emocao = Emocao(
                        memoria_id=mems_recentes[0].id_memorias,
                        emocao="Satisfação",
                        intensidade=0.3
                    )
                    session.add(emocao)
                    self._log_operation("EMOCAO_INTENCAO_SATISFEITA", resultado=f"{i.descricao}", current_session=session)
                elif not mems_recentes and idade_horas >= 2:
                    logging.warning(f"❌ Intenção '{i.descricao}' parece frustrada (sem progresso após {int(idade_horas)}h)")
                    emocao = Emocao(
                        memoria_id=None,
                        emocao="Frustração",
                        intensidade=0.4
                    )
                    session.add(emocao)
                    self._log_operation("EMOCAO_INTENCAO_FRUSTRADA", resultado=f"{i.descricao}", current_session=session)

            session.commit()
            self._log_operation("AVALIAR_SATISFACAO", resultado=f"{len(intencoes)} intenções avaliadas", current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("AVALIAR_SATISFACAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao avaliar satisfação das intenções: {e}")
        finally:
            session.close()

    def avaliar_motivacao_intencoes(self):
        session = self._get_session()
        try:
            agora = datetime.now()
            intencoes = self.obter_intencoes_ativas(session=session)

            for i in intencoes:
                motivacao = i.motivacao or 0.0

                try:
                    criado_em_dt = datetime.strptime(i.criado_em, "%Y-%m-%d %H:%M:%S.%f")
                except ValueError:
                    try:
                        criado_em_dt = datetime.fromisoformat(i.criado_em)
                    except ValueError:
                        logging.warning(f"⚠️ Aviso: Não foi possível analisar a string de data/hora '{i.criado_em}'. Pulando intenção.")
                        self._log_operation("ERROR_DATETIME_PARSE", resultado=f"Formato inválido para {i.descricao}", current_session=session)
                        continue

                mems_relacionadas = session.query(Memoria).filter(
                    or_(
                        Memoria.conteudo.ilike(f"%{i.descricao}%"),
                        Memoria.contexto.ilike(f"%{i.descricao}%")
                    ),
                    Memoria.tempo >= criado_em_dt.isoformat()
                ).all()

                if mems_relacionadas:
                    motivacao += 0.1 * len(mems_relacionadas)

                for mem in mems_relacionadas:
                    for emo in mem.emocoes:
                        if emo.intensidade >= 0.5:
                            motivacao += 0.15

                reflexoes = session.query(Reflexao).filter(
                    Reflexao.conteudo.ilike(f"%{i.descricao}%")
                ).all()

                motivacao += 0.1 * len(reflexoes)

                idade = agora - criado_em_dt
                horas = idade.total_seconds() / 3600
                if horas > 6:
                    motivacao -= 0.1 * (horas // 2)

                motivacao = max(0.0, min(motivacao, 1.0))
                i.motivacao = motivacao

                if motivacao > 0.7:
                    label = "🧠 Persistente"
                elif motivacao > 0.3:
                    label = "🎯 Engajada"
                else:
                    label = "🌀 Interesse Frágil"

                logging.info(f"📊 [{i.id_intencao}] {i.descricao} → Motivação: {motivacao:.2f} ({label})")

            session.commit()
            self._log_operation("AVALIAR_MOTIVACAO", resultado=f"{len(intencoes)} intenções avaliadas", current_session=session)
        except Exception as e:
            session.rollback()
            self._log_operation("AVALIAR_MOTIVACAO_FAILED", resultado=f"Falha: {e}", current_session=session)
            logging.error(f"Erro ao avaliar motivação das intenções: {e}")
        finally:
            session.close()

    def salvar_interacao_no_banco(self, interacao_dict):
        session = self._get_session()
        try:
            person_id = interacao_dict.get("id_pessoa")
            if not person_id:
                pessoa = session.query(Pessoa).filter_by(nome_pessoa="deyvid").first()
                if not pessoa:
                    pessoa = Pessoa(nome_pessoa="deyvid")
                    session.add(pessoa)
                    session.flush()
                person_id = pessoa.id_pessoa

            nova_memoria = Memoria(
                conteudo=interacao_dict["text_output"] or "Sem resposta.",
                contexto=interacao_dict["text_input"] or "",
                emocao=interacao_dict.get("emotion_alyssa", {}).get("nome", "neutra"),
                tempo=interacao_dict["timestamp"],
                origem="conversa",
                tipo=interacao_dict.get("tipo", "interacao"),
                importancia=interacao_dict.get("importancia", 0.5),
                id_pessoa=person_id,
                id_area=interacao_dict.get("id_area"),
                id_estado_emocional=interacao_dict.get("id_estado_emocional")
            )
            session.add(nova_memoria)
            session.flush()

            if interacao_dict.get("emotion_alyssa"):
                e = interacao_dict["emotion_alyssa"]
                emocao = Emocao(
                    memoria_id=nova_memoria.id_memorias,
                    emocao=e.get("nome", "neutra"),
                    intensidade=e.get("intensidade", 0.5)
                )
                session.add(emocao)

            if interacao_dict.get("decision"):
                auto = AutoAvaliacao(
                    tipo="decisao",
                    descricao=interacao_dict["decision"],
                    evidencia_memoria_id=nova_memoria.id_memorias,
                    criado_em=datetime.now().isoformat()
                )
                session.add(auto)

            if interacao_dict.get("actions"):
                for a in interacao_dict["actions"]:
                    entrada = EntradaSensor(
                        tipo="acao",
                        conteudo=str(a),
                        tempo=datetime.now().isoformat(),
                        vinculado_a=nova_memoria.id_memorias
                    )
                    session.add(entrada)

            if interacao_dict.get("motor_output"):
                entrada = EntradaSensor(
                    tipo="motor_output",
                    conteudo=str(interacao_dict["motor_output"]),
                    tempo=datetime.now().isoformat(),
                    vinculado_a=nova_memoria.id_memorias
                )
                session.add(entrada)

            session.commit()
            self._log_operation("SAVE_INTERACTION", resultado=f"Memória ID:{nova_memoria.id_memorias}", current_session=session)
            logging.info(f"🧠 Interação salva no banco. Memória ID: {nova_memoria.id_memorias}")
            return nova_memoria.id_memorias
        except Exception as e:
            session.rollback()
            self._log_operation("SAVE_INTERACTION_FAILED", resultado=f"Falha: {e}", contexto_entrada=str(interacao_dict), current_session=session)
            logging.error(f"❌ Erro ao salvar interação no banco: {e}")
            return None
        finally:
            session.close()


class MemorySystem:
    def __init__(self, db_path="sqlite:///memory_system/memoria_db.sqlite"):
        self.handler = DatabaseHandler(db_path)

    def save_memory(self, user_input: str, alyssa_response: str, dialogue: str, emotion_score: dict, tipo: str,
                    area_name: str = None, person_name: str = "deyvid", importance: float = 0.5,
                    decision: str = None, actions: list = None, motor_output: str = None):
        """
        Saves a comprehensive interaction memory into the system.

        :param user_input: The text input from the user.
        :param alyssa_response: Alyssa's text response.
        :param dialogue: The overall dialogue context.
        :param emotion_score: A dictionary of emotions and their scores (e.g., {"joy": 0.6, "trust": 0.4}).
        :param tipo: The type of memory (e.g., "interacao", "pensamento", "evento").
        :param area_name: Optional name of the area associated with the memory.
        :param person_name: Optional name of the person involved (defaults to "deyvid").
        :param importance: Importance score of the memory (defaults to 0.5).
        :param decision: Optional decision made during the interaction.
        :param actions: Optional list of actions taken during the interaction.
        :param motor_output: Optional motor output generated.
        :return: The ID of the created memory, or None if an error occurred.
        """
        current_timestamp = datetime.now().isoformat()

        # Get or create person using handler method
        person = self.handler.get_person_by_name(person_name)
        if not person:
            person_id = self.handler.create_person(nome_pessoa=person_name)
        else:
            person_id = person.id_pessoa

        # Get or create area using handler method
        area_id = None
        if area_name:
            area = self.handler.get_area_by_name(area_name)
            if not area:
                area_id = self.handler.create_area(nome=area_name)
            else:
                area_id = area.id_area

        # Determine dominant emotion and its intensity for memory creation
        dominant_emotion_name = "neutra"
        dominant_emotion_intensity = 0.0
        if emotion_score:
            dominant_emotion_name = max(emotion_score, key=emotion_score.get)
            dominant_emotion_intensity = emotion_score[dominant_emotion_name]

        # Get or create estado_emocional_id using handler method
        estado_emocional_id = None
        if dominant_emotion_name != "neutra":
            estado_emocional = self.handler.get_estado_emocional_by_name(dominant_emotion_name)
            if not estado_emocional:
                estado_emocional_id = self.handler.create_estado_emocional(nome=dominant_emotion_name, intensidade=dominant_emotion_intensity)
            else:
                estado_emocional_id = estado_emocional.id_estado

        # Prepare dict for salvar_interacao_no_banco
        interacao_dict_for_save = {
            "text_input": user_input,
            "text_output": alyssa_response,
            "timestamp": current_timestamp,
            "emotion_alyssa": {"nome": dominant_emotion_name, "intensidade": dominant_emotion_intensity},
            "decision": decision,
            "actions": actions,
            "motor_output": motor_output,
            "tipo": tipo, # Pass tipo
            "importancia": importance, # Pass importance
            "id_area": area_id,
            "id_pessoa": person_id,
            "id_estado_emocional": estado_emocional_id
        }

        memory_id = self.handler.salvar_interacao_no_banco(interacao_dict_for_save)
        return memory_id

    def retrieve_memories(self, query: str, limit: int = 5, search_type: str = "keyword"):
        """
        Retrieves memories based on a query.

        :param query: The query string or embedding.
        :param limit: Maximum number of memories to return.
        :param search_type: "keyword" for content/context search, "semantic" for vector search.
        :return: A list of Memory objects.
        """
        if search_type == "keyword":
            query_params = {"conteudo_like": f"%{query}%", "contexto_like": f"%{query}%"}
            return self.handler.search_memories(query_params, limit=limit)
        elif search_type == "semantic":
            return self.handler.semantic_search_memories(query, top_k=limit)
        else:
            logging.error("Invalid search_type. Use 'keyword' or 'semantic'.")
            return []

    def get_recent_memories(self, limit: int = 10, hours_ago: int = 24):
        """
        Retrieves recent memories.

        :param limit: Maximum number of memories to return.
        :param hours_ago: How many hours back to search for memories.
        :return: A list of dictionaries representing recent memories.
        """
        return self.handler.search_recent_memories(limit=limit, hours_ago=hours_ago)

    def get_memory_details(self, memory_id: int):
        """
        Retrieves full details of a specific memory by its ID.

        :param memory_id: The ID of the memory.
        :return: A Memory object or None.
        """
        return self.handler.get_memory_by_id(memory_id)

    def update_memory_details(self, memory_id: int, updates: dict):
        """
        Updates specific fields of an existing memory.

        :param memory_id: The ID of the memory to update.
        :param updates: A dictionary of fields to update (e.g., {"importance": 0.9}).
        :return: True if successful, False otherwise.
        """
        return self.handler.update_memory(memory_id, updates)

    def delete_memory_entry(self, memory_id: int):
        """
        Deletes a memory entry from the system.

        :param memory_id: The ID of the memory to delete.
        :return: True if successful, False otherwise.
        """
        return self.handler.delete_memory(memory_id)

    def manage_intentions(self, action: str, **kwargs):
        """
        Manages intentions (activate, get active, deactivate, analyze for).

        :param action: "activate", "get_active", "deactivate", "analyze_input".
        :param kwargs: arguments specific to the action.
            For "activate": description, tipo, gatilho (optional), boost (optional).
            For "deactivate": id_intencao.
            For "analyze_input": user_input.
        :return: Varies based on action (e.g., ID for activate, list for get_active).
        """
        if action == "activate":
            return self.handler.ativar_intencao(
                kwargs.get("descricao"),
                kwargs.get("tipo"),
                kwargs.get("gatilho"),
                kwargs.get("boost", 0.0)
            )
        elif action == "get_active":
            return self.handler.obter_intencoes_ativas()
        elif action == "deactivate":
            return self.handler.encerrar_intencao(kwargs.get("id_intencao"))
        elif action == "analyze_input":
            self.handler.analisar_entrada_para_intencao(kwargs.get("user_input"))
        else:
            logging.error(f"Invalid intention action: {action}")
            return None

    def decay_all_memories(self):
        """Applies the decay function to all memories in the system."""
        self.handler.decair_memorias()

    def generate_reflections(self):
        """Triggers the reflection generation process based on recent emotional memories."""
        self.handler.gerar_reflexoes()

    def evaluate_intentions_satisfaction_and_motivation(self):
        """Evaluates the satisfaction and motivation levels of active intentions."""
        self.handler.avaliar_satisfacao_intencoes()
        self.handler.avaliar_motivacao_intencoes()

    def check_for_auto_activation(self):
        """Checks if any intentions should be auto-activated based on current state and recent interactions."""
        self.handler.verificar_auto_ativacao()


# --- Exemplo de Uso de High-Level ---
if __name__ == "__main__":
    # Inicializa o sistema de memória
    memory = MemorySystem(db_path="sqlite:///memory/memoria_db.sqlite")

    # Exemplo 1: Salvar uma interação
    print("\n--- Exemplo 1: Salvando uma interação ---")
    memory_id_1 = memory.save_memory(
        user_input="Eai Alyssa, como você está?",
        alyssa_response="Ah, que bom te ouvir! Estou me sentindo bem e curiosa hoje.",
        dialogue="O usuário perguntou sobre o estado de Alyssa e ela respondeu com curiosidade.",
        emotion_score={"alegria": 0.7, "curiosidade": 0.5},
        tipo="interacao",
        area_name="Conversa Cotidiana",
        person_name="Deyvid",
        importance=0.8,
        decision="Priorizar engajamento com Deyvid.",
        actions=["analisar_sentimento_usuario", "formular_resposta_positiva"],
        motor_output="nenhum"
    )
    if memory_id_1:
        print(f"Interação salva com sucesso! ID da Memória: {memory_id_1}")

    # Exemplo 2: Recuperar memórias recentes
    print("\n--- Exemplo 2: Recuperando memórias recentes ---")
    recent_mems = memory.get_recent_memories(limit=3, hours_ago=1)
    if recent_mems:
        print("Memórias recentes:")
        for m in recent_mems:
            print(f"- Tipo: {m['tipo']}, Conteúdo: '{m['text_input']}', Emoção: {m['emocao']}")
    else:
        print("Nenhuma memória recente encontrada.")

    # Exemplo 3: Recuperar detalhes de uma memória específica
    print(f"\n--- Exemplo 3: Recuperando detalhes da Memória ID: {memory_id_1} ---")
    if memory_id_1:
        details = memory.get_memory_details(memory_id_1)
        if details:
            print(f"Detalhes da Memória {memory_id_1}:")
            print(f"  Conteúdo: {details.conteudo}")
            print(f"  Contexto: {details.contexto}")
            print(f"  Emoção: {details.emocao}")
            print(f"  Origem: {details.origem}")
            print(f"  Importância: {details.importancia}")
            if details.pessoa_rel:
                print(f"  Pessoa Associada: {details.pessoa_rel.nome_pessoa}")
        else:
            print(f"Memória com ID {memory_id_1} não encontrada.")

    # Exemplo 4: Buscar memórias por palavra-chave
    print("\n--- Exemplo 4: Buscando memórias por palavra-chave 'Alyssa' ---")
    found_by_keyword = memory.retrieve_memories(query="Alyssa", search_type="keyword", limit=2)
    if found_by_keyword:
        print("Memórias encontradas por palavra-chave:")
        for m in found_by_keyword:
            print(f"- ID: {m.id_memorias}, Conteúdo: '{m.conteudo[:50]}...'")
    else:
        print("Nenhuma memória encontrada com a palavra-chave.")

    # Exemplo 5: Ativar e gerenciar intenções
    print("\n--- Exemplo 5: Gerenciando Intenções ---")
    # Ativar uma nova intenção
    intention_id = memory.manage_intentions(
        action="activate",
        descricao="Aprender mais sobre IA",
        tipo="aprendizado",
        gatilho="usuário perguntou sobre IA",
        boost=0.6
    )
    if intention_id:
        print(f"Intenção 'Aprender mais sobre IA' ativada com ID: {intention_id}")

    # Obter intenções ativas
    active_intentions = memory.manage_intentions(action="get_active")
    if active_intentions:
        print("Intenções ativas atuais:")
        for i in active_intentions:
            print(f"- {i.descricao} (ID: {i.id_intencao})")

    # Analisar entrada para auto-ativar intenções
    print("\n--- Analisando entrada para auto-ativação de intenções ---")
    memory.manage_intentions(action="analyze_input", user_input="Eu queria entender sobre sensores de tato.")
    active_intentions_after_analysis = memory.manage_intentions(action="get_active")
    if active_intentions_after_analysis:
        print("Intenções ativas após análise de entrada:")
        for i in active_intentions_after_analysis:
            print(f"- {i.descricao} (ID: {i.id_intencao})")


    # Exemplo 6: Simular decaimento de memórias
    print("\n--- Exemplo 6: Simulando decaimento de memórias ---")
    # Create a low importance memory to see decay effect
    low_imp_mem = memory.handler.create_memory(
        tipo="Fato",
        conteudo="Um fato sem importância para esquecer",
        contexto="Teste",
        emocao="Neutra",
        origem="Sistema",
        importancia=0.1
    )
    if low_imp_mem:
        print(f"Memória de baixa importância criada: ID {low_imp_mem.id_memorias}")
        # Manually set a past last access time to simulate decay over time
        session_decay = memory.handler._get_session()
        decay_entry = session_decay.query(Decadencia).filter_by(id_memoria=low_imp_mem.id_memorias).first()
        if decay_entry:
            decay_entry.ultimo_acesso = datetime.now() - timedelta(hours=50) # Simulate 50 hours passed
            session_decay.commit()
            print("Tempo de último acesso da memória de baixa importância ajustado para simular decaimento.")
        session_decay.close()

    memory.decay_all_memories()

    if low_imp_mem:
        updated_low_imp_mem = memory.get_memory_details(low_imp_mem.id_memorias)
        if updated_low_imp_mem and updated_low_imp_mem.decadencia:
            print(f"Nova importância da memória {low_imp_mem.id_memorias} após decaimento: "
                  f"{updated_low_imp_mem.decadencia.valor_atual:.2f}")

    # Exemplo 7: Gerar Reflexões
    print("\n--- Exemplo 7: Gerando Reflexões ---")
    # To see reflections, ensure you have recent memories with emotions and an active emotional state.
    # Let's create a few "alegria" memories to trigger the reflection.
    _ = memory.save_memory(user_input="Dia feliz hoje!", alyssa_response="Que bom!", dialogue="Feedback positivo", emotion_score={"alegria": 0.9}, tipo="evento")
    _ = memory.save_memory(user_input="Adorei o passeio.", alyssa_response="Perfeito!", dialogue="Mais feedback positivo", emotion_score={"alegria": 0.8}, tipo="evento")
    _ = memory.save_memory(user_input="Estou radiante!", alyssa_response="Maravilhoso!", dialogue="Alegria contagiante", emotion_score={"alegria": 0.95}, tipo="evento")

    # Register current emotional state (e.g., happiness)
    # First, ensure 'Alegria' emotional state exists
    estado_alegria_id = memory.handler.create_estado_emocional(nome="Alegria", intensidade=0.9)
    if estado_alegria_id:
        memory.handler.registrar_estado_emocional_atual(id_estado=estado_alegria_id)
        print(f"Estado emocional atual registrado como Alegria (ID: {estado_alegria_id}).")
    
    memory.generate_reflections()

    # Exemplo 8: Avaliar satisfação e motivação das intenções
    print("\n--- Exemplo 8: Avaliando satisfação e motivação das intenções ---")
    # This will use the "Aprender mais sobre IA" intention created earlier
    memory.evaluate_intentions_satisfaction_and_motivation()

    # Exemplo 9: Verificar auto-ativação de intenções
    print("\n--- Exemplo 9: Verificando auto-ativação de intenções ---")
    memory.check_for_auto_activation()

    # Exemplo 10: Encerrar uma intenção
    if intention_id:
        print(f"\n--- Exemplo 10: Encerrando intenção com ID: {intention_id} ---")
        memory.manage_intentions(action="deactivate", id_intencao=intention_id)
        active_intentions_final = memory.manage_intentions(action="get_active")
        if active_intentions_final:
            print("Intenções ativas após encerramento:")
            for i in active_intentions_final:
                print(f"- {i.descricao} (ID: {i.id_intencao})")
        else:
            print("Nenhuma intenção ativa restante.")